// Copyright (c) 2026 Roberto Raggi <roberto.raggi@gmail.com>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include <cxx/ast.h>
#include <cxx/attributes.h>
#include <cxx/binder.h>
#include <cxx/control.h>
#include <cxx/dependent_types.h>
#include <cxx/memory_layout.h>
#include <cxx/names.h>
#include <cxx/symbols.h>
#include <cxx/type_traits.h>
#include <cxx/types.h>
#include <cxx/util.h>
#include <cxx/views/symbols.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <format>
#include <memory>
#include <optional>
#include <ranges>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace cxx {
[[nodiscard]] static auto virtualBasesInInheritanceGraphOrder(
    ClassSymbol* classSymbol) -> std::vector<ClassSymbol*> {
  struct InheritanceFrame {
    ClassSymbol* classSymbol;
    std::size_t nextBase = 0;
  };

  std::vector<ClassSymbol*> virtualBases;
  std::vector<InheritanceFrame> frames{{classSymbol}};
  while (!frames.empty()) {
    auto& frame = frames.back();
    auto& bases = frame.classSymbol->baseClasses();
    if (frame.nextBase == bases.size()) {
      frames.pop_back();
      continue;
    }
    auto base = bases[frame.nextBase++];
    auto baseClass = resolved_base_class(base);
    if (!baseClass) continue;
    if (base->isVirtual()) {
      if (std::ranges::contains(virtualBases, baseClass)) continue;
      virtualBases.push_back(baseClass);
    }
    frames.push_back({baseClass});
  }
  return virtualBases;
}

[[nodiscard]] static auto indirectPrimaryBasesOf(ClassSymbol* classSymbol)
    -> std::vector<ClassSymbol*> {
  std::vector<ClassSymbol*> primaryBases;
  std::vector<ClassSymbol*> visited{classSymbol};

  for (std::size_t index = 0; index < visited.size(); ++index) {
    for (auto baseClass : visited[index]->baseClasses()) {
      auto base = resolved_base_class(baseClass);
      if (!base) continue;
      if (std::ranges::contains(visited, base)) continue;
      visited.push_back(base);

      auto baseLayout = base->layout();
      if (!baseLayout || !baseLayout->primaryBaseIsVirtual()) continue;
      if (std::ranges::contains(primaryBases, baseLayout->primaryBase()))
        continue;
      primaryBases.push_back(baseLayout->primaryBase());
    }
  }

  return primaryBases;
}

struct [[nodiscard]] Binder::BuildRecordLayout {
  Binder& binder;
  ClassSymbol* classSymbol;
  const MemoryLayout* memoryLayout;
  std::unique_ptr<ClassLayout> layout;

  int calculatedSize = 0;
  int calculatedAlignment = 1;
  std::uint64_t runningSizeof = 0;
  std::uint64_t emptyComponentEnd = 0;
  std::uint64_t emittedEnd = 0;
  std::uint32_t currentIndex = 0;

  int nextBitPos = 0;
  int runStartByte = 0;
  std::uint32_t runIndex = 0;
  bool inBitfieldRun = false;
  std::vector<FieldSymbol*> runFields;
  ClassSubobjectList placedClassSubobjects;
  std::uint64_t maxPlacedSubobjectOffset = 0;
  std::vector<std::pair<ClassSymbol*, ClassLayout::MemberInfo>>
      indirectPrimaryPlacements;

  int packValue = 0;

  BuildRecordLayout(Binder& b, ClassSymbol* cls)
      : binder(b),
        classSymbol(cls),
        memoryLayout(b.control()->memoryLayout()),
        layout(std::make_unique<ClassLayout>()) {
    packValue = cls->packAlignment();
  }

  auto control() const -> Control* { return binder.control(); }

  auto operator()() -> std::expected<bool, std::string>;
  auto validate() -> std::expected<bool, std::string>;
  void completeFieldTypes();
  [[nodiscard]] auto computeAbiEmpty() const -> bool;
  [[nodiscard]] auto hasOnlyZeroSizeDataMembers(ClassSymbol* candidate) const
      -> bool;
  [[nodiscard]] auto isNearlyEmptyClass(ClassSymbol* classSymbol) const -> bool;
  [[nodiscard]] auto selectPrimaryBase() const -> std::pair<ClassSymbol*, bool>;
  void padTo(std::uint64_t offset);
  void layoutVtable();
  void layoutBases();
  void layoutVirtualBases();
  void recordIndirectPrimaryPlacement(ClassSymbol* primary,
                                      ClassLayout::MemberInfo info);
  [[nodiscard]] auto indirectPrimaryPlacement(ClassSymbol* primary) const
      -> std::optional<ClassLayout::MemberInfo>;
  void recordPrimaryChain(ClassSymbol* cls, std::uint64_t offset,
                          std::uint32_t topIndex);
  void collectIndirectPrimaryPlacements(ClassSymbol* root,
                                        std::uint64_t rootOffset,
                                        std::uint32_t rootIndex);
  [[nodiscard]] auto baseNonVirtualSize(ClassSymbol* base) -> std::uint64_t;
  [[nodiscard]] auto allocateBaseSubobject(ClassSymbol* base, bool isVirtual)
      -> ClassLayout::MemberInfo;
  void growSizeof(std::uint64_t offset, std::uint64_t sizeInBytes);
  void recordEmptyComponent(std::uint64_t offset, std::uint64_t sizeInBytes);
  [[nodiscard]] static auto subobjectCovers(const ClassSubobject& subobject,
                                            std::uint64_t address) -> bool;

  [[nodiscard]] static auto subobjectLast(const ClassSubobject& subobject)
      -> std::uint64_t;

  [[nodiscard]] auto placedAt(ClassSymbol* symbol, std::uint64_t address) const
      -> bool;

  [[nodiscard]] auto conflictsAt(const ClassSubobject& candidate,
                                 std::size_t level, std::uint64_t address) const
      -> bool;

  [[nodiscard]] auto classSubobjectOffset(ClassSymbol* classSymbol,
                                          bool tryZero, std::uint64_t alignment)
      -> std::uint64_t;
  void recordNonVirtualClassSubobjects(ClassSymbol* classSymbol,
                                       std::uint64_t offset,
                                       ClassSubobjectExtent extent = {});
  auto layoutFields() -> std::expected<bool, std::string>;
  auto layoutBitfield(FieldSymbol* field) -> std::expected<bool, std::string>;
  void layoutZeroWidthBitfield(FieldSymbol* field);
  auto layoutRegularField(FieldSymbol* field)
      -> std::expected<bool, std::string>;
  void closeBitfieldRun();
  [[nodiscard]] auto isPackedClass() const -> bool;
  [[nodiscard]] auto packAlignment(int alignment) const -> int;
  [[nodiscard]] auto keepsBitFieldInAllocationUnit(FieldSymbol* field) const
      -> bool;
  void propagateBaseFields();
  void propagateAnonymousFields(ClassSymbol* owner,
                                const ClassLayout* ownerLayout,
                                std::uint64_t ownerOffset);
  void copyFieldInfos(ClassSymbol* owner, const ClassLayout* ownerLayout,
                      std::uint64_t ownerOffset);
  void finalize();
  void buildVTableLayout();
};

auto Binder::buildRecordLayout(ClassSymbol* classSymbol)
    -> std::expected<bool, std::string> {
  return BuildRecordLayout{*this, classSymbol}();
}

auto Binder::BuildRecordLayout::operator()()
    -> std::expected<bool, std::string> {
  if (auto status = validate(); !status) return status;

  completeFieldTypes();

  layout->setAbiEmpty(computeAbiEmpty());
  layoutVtable();
  layoutBases();

  auto fieldsStatus = layoutFields();
  if (!fieldsStatus) return fieldsStatus;
  if (!fieldsStatus.value()) return false;

  auto nonVirtualSize = static_cast<std::uint64_t>(calculatedSize);
  if (emptyComponentEnd > nonVirtualSize) nonVirtualSize = emptyComponentEnd;

  layout->setNonVirtualSize(nonVirtualSize);
  layout->setNonVirtualAlignment(calculatedAlignment);

  layoutVirtualBases();

  propagateBaseFields();
  finalize();

  return true;
}

auto Binder::BuildRecordLayout::validate() -> std::expected<bool, std::string> {
  if (memoryLayout->usesMicrosoftBitFieldLayout() &&
      std::ranges::any_of(
          views::members(classSymbol) | views::non_static_fields,
          [](FieldSymbol* field) { return field->isBitField(); })) {
    return std::unexpected("Microsoft ABI bit-field layout is not supported");
  }

  for (auto base : classSymbol->baseClasses()) {
    auto baseClassSymbol = symbol_cast<ClassSymbol>(base->symbol());
    if (!baseClassSymbol) {
      return std::unexpected(
          std::format("base class '{}' not found", to_string(base->name())));
    }
    if (!baseClassSymbol->isComplete()) {
      binder.traits.requireCompleteClass(baseClassSymbol);
    }
    baseClassSymbol = baseClassSymbol->resolvedDefinition();
    if (!baseClassSymbol->isComplete()) {
      return std::unexpected(std::format("base class '{}' is incomplete",
                                         to_string(baseClassSymbol->name())));
    }
  }
  return true;
}

void Binder::BuildRecordLayout::completeFieldTypes() {
  for (auto field : views::members(classSymbol) | views::non_static_fields) {
    auto fieldElementType =
        binder.traits.remove_cv(binder.traits.remove_all_extents(
            binder.traits.remove_cv(field->type())));

    auto classType = type_cast<ClassType>(fieldElementType);
    if (!classType) continue;

    binder.traits.requireCompleteClass(classType->symbol());

    if (auto alignment =
            binder.control()->memoryLayout()->alignmentOf(field->type())) {
      field->setAlignment(alignment.value());
    }
  }
}

auto Binder::BuildRecordLayout::hasOnlyZeroSizeDataMembers(
    ClassSymbol* candidate) const -> bool {
  return std::ranges::all_of(
      views::members(candidate) | views::non_static_fields,
      [this](FieldSymbol* field) {
        return binder.traits.is_zero_size_subobject(field);
      });
}

auto Binder::BuildRecordLayout::computeAbiEmpty() const -> bool {
  if (views::any_function(classSymbol->members(),
                          [](FunctionSymbol* f) { return f->isVirtual(); }))
    return false;

  if (!hasOnlyZeroSizeDataMembers(classSymbol)) return false;

  for (auto base : classSymbol->baseClasses()) {
    if (base->isVirtual()) return false;
    auto baseClass = symbol_cast<ClassSymbol>(base->symbol());
    if (!baseClass) return false;
    auto baseLayout = baseClass->resolvedDefinition()->layout();
    if (!baseLayout || !baseLayout->isAbiEmpty()) return false;
  }
  return true;
}

void Binder::BuildRecordLayout::layoutVtable() {
  if (classSymbol->isUnion()) return;

  const auto hasVirtualFunction = views::any_function(
      classSymbol->members(), [](FunctionSymbol* f) { return f->isVirtual(); });
  const auto hasDynamicBase = std::ranges::any_of(
      classSymbol->baseClasses(), [](BaseClassSymbol* base) {
        auto baseClass = resolved_base_class(base);
        return baseClass && baseClass->layout() &&
               baseClass->layout()->hasVtable();
      });
  const auto hasVirtualBase = std::ranges::any_of(
      classSymbol->baseClasses(),
      [](BaseClassSymbol* base) { return base->isVirtual(); });
  if (!hasVirtualFunction && !hasDynamicBase && !hasVirtualBase) return;

  layout->setHasVtable(true);
  auto [primaryBase, primaryIsVirtual] = selectPrimaryBase();
  if (primaryBase) {
    layout->setPrimaryBase(primaryBase, primaryIsVirtual);
    if (!primaryIsVirtual) return;

    ClassLayout::MemberInfo primaryInfo;
    primaryInfo.index = currentIndex++;
    layout->setVirtualBaseInfo(primaryBase, primaryInfo);
    layout->setHasDirectVtable(true);
    layout->setVtableIndex(primaryInfo.index);
    recordNonVirtualClassSubobjects(primaryBase, 0);
  } else {
    layout->setHasDirectVtable(true);
    layout->setVtableIndex(currentIndex++);
  }

  auto ptrSize = static_cast<int>(memoryLayout->sizeOfPointer());
  calculatedSize = ptrSize;
  calculatedAlignment = packAlignment(isPackedClass() ? 1 : ptrSize);
  emittedEnd = static_cast<std::uint64_t>(ptrSize);
  nextBitPos = calculatedSize * 8;
}

auto Binder::BuildRecordLayout::isNearlyEmptyClass(ClassSymbol* candidate) const
    -> bool {
  if (!candidate) return false;
  candidate = candidate->resolvedDefinition();
  auto candidateLayout = candidate->layout();
  if (!candidateLayout || !candidateLayout->hasVtable()) return false;

  if (!hasOnlyZeroSizeDataMembers(candidate)) return false;

  int nearlyEmptyNonVirtualBases = 0;
  for (auto base : candidate->baseClasses()) {
    if (base->isVirtual()) continue;
    auto baseClass = symbol_cast<ClassSymbol>(base->symbol());
    if (!baseClass) return false;
    baseClass = baseClass->resolvedDefinition();
    auto baseLayout = baseClass->layout();
    if (baseLayout && baseLayout->isAbiEmpty()) continue;
    if (!isNearlyEmptyClass(baseClass)) return false;
    if (++nearlyEmptyNonVirtualBases > 1) return false;
  }

  std::vector<std::pair<ClassSymbol*, std::uint64_t>> pendingBases{
      {candidate, 0}};
  while (!pendingBases.empty()) {
    auto [cls, offset] = pendingBases.back();
    pendingBases.pop_back();
    auto classLayout = cls->layout();
    if (!classLayout) continue;
    for (auto base : cls->baseClasses()) {
      if (base->isVirtual()) continue;
      auto baseClass = resolved_base_class(base);
      if (!baseClass) continue;
      auto info = classLayout->getBaseInfo(baseClass);
      if (!info) continue;
      const auto baseOffset = offset + info->offset;
      auto baseLayout = baseClass->layout();
      if (baseOffset != 0 && baseLayout && baseLayout->isAbiEmpty())
        return false;
      pendingBases.emplace_back(baseClass, baseOffset);
    }
  }
  return true;
}

auto Binder::BuildRecordLayout::selectPrimaryBase() const
    -> std::pair<ClassSymbol*, bool> {
  for (auto base : classSymbol->baseClasses()) {
    if (base->isVirtual()) continue;
    auto baseClass = resolved_base_class(base);
    if (baseClass && baseClass->layout() && baseClass->layout()->hasVtable())
      return {baseClass, false};
  }

  auto indirectPrimaryBases = indirectPrimaryBasesOf(classSymbol);

  std::vector<ClassSymbol*> candidates;
  for (auto virtualBase : virtualBasesInInheritanceGraphOrder(classSymbol)) {
    if (isNearlyEmptyClass(virtualBase)) candidates.push_back(virtualBase);
  }

  auto candidate = std::ranges::find_if(candidates, [&](ClassSymbol* base) {
    return !std::ranges::contains(indirectPrimaryBases, base);
  });
  if (candidate != candidates.end()) return {*candidate, true};
  if (!candidates.empty()) return {candidates.front(), true};
  return {nullptr, false};
}

auto Binder::BuildRecordLayout::baseNonVirtualSize(ClassSymbol* base)
    -> std::uint64_t {
  if (!base->layout()) return base->sizeInBytes();
  return binder.traits.non_virtual_size(base->type());
}

auto Binder::fieldElementClass(FieldSymbol* field) -> ClassSymbol* {
  auto elementType = traits.remove_cv(traits.remove_all_extents(field->type()));
  auto classType = type_cast<ClassType>(elementType);
  if (!classType || !classType->symbol()) return nullptr;
  return classType->symbol()->resolvedDefinition();
}

auto Binder::fieldArrayExtent(FieldSymbol* field) -> ClassSubobjectExtent {
  ClassSubobjectExtent extent;
  if (!traits.is_array(field->type())) return extent;

  auto elementType = traits.remove_cv(traits.remove_all_extents(field->type()));
  auto memoryLayout = control()->memoryLayout();

  const auto elementSize = memoryLayout->sizeOf(elementType);
  const auto totalSize = memoryLayout->sizeOf(field->type());

  if (!elementSize.has_value() || !totalSize.has_value()) return extent;
  if (!elementSize.value()) return extent;

  extent.stride = elementSize.value();
  extent.count = totalSize.value() / elementSize.value();
  return extent;
}

void Binder::appendClassSubobjects(ClassSubobjectList& subobjects,
                                   ClassSymbol* classSymbol,
                                   std::uint64_t offset,
                                   ClassSubobjectExtent extent) {
  if (!extent.count) return;

  for (const auto& subobject : emptyClassSubobjects(classSymbol)) {
    auto& merged = subobjects.emplace_back(subobject);
    merged.offset += offset;
    if (extent.count > 1) merged.extents.insert(merged.extents.begin(), extent);
  }
}

auto Binder::emptyClassSubobjects(ClassSymbol* classSymbol)
    -> const ClassSubobjectList& {
  classSymbol = classSymbol->resolvedDefinition();

  const auto layout = classSymbol->layout();

  if (auto it = emptyClassSubobjects_.find(classSymbol);
      it != emptyClassSubobjects_.end() && it->second.layout == layout) {
    return it->second.subobjects;
  }

  ClassSubobjectList subobjects;

  if (layout && layout->isAbiEmpty()) {
    subobjects.emplace_back(classSymbol, 0,
                            std::vector<ClassSubobjectExtent>{});
  }

  if (layout) {
    for (auto base : classSymbol->baseClasses()) {
      if (base->isVirtual()) continue;
      auto baseClass = resolved_base_class(base);
      if (!baseClass) continue;
      if (auto baseInfo = layout->getBaseInfo(baseClass)) {
        appendClassSubobjects(subobjects, baseClass, baseInfo->offset,
                              ClassSubobjectExtent{});
      }
    }

    for (auto field : views::members(classSymbol) | views::non_static_fields) {
      auto fieldClass = fieldElementClass(field);
      if (!fieldClass || !fieldClass->layout()) continue;
      auto fieldInfo = layout->getFieldInfo(field);
      if (!fieldInfo) continue;

      appendClassSubobjects(subobjects, fieldClass, fieldInfo->offset,
                            fieldArrayExtent(field));
    }
  }

  auto& entry = emptyClassSubobjects_[classSymbol];
  entry.layout = layout;
  entry.subobjects = std::move(subobjects);
  return entry.subobjects;
}

auto Binder::BuildRecordLayout::subobjectCovers(const ClassSubobject& subobject,
                                                std::uint64_t address) -> bool {
  if (address < subobject.offset) return false;
  auto displacement = address - subobject.offset;
  for (const auto& extent : subobject.extents) {
    if (!extent.stride) continue;
    const auto index = displacement / extent.stride;
    if (index >= extent.count) return false;
    displacement -= index * extent.stride;
  }
  return displacement == 0;
}

auto Binder::BuildRecordLayout::subobjectLast(const ClassSubobject& subobject)
    -> std::uint64_t {
  auto last = subobject.offset;
  for (const auto& extent : subobject.extents)
    last += (extent.count - 1) * extent.stride;
  return last;
}

auto Binder::BuildRecordLayout::placedAt(ClassSymbol* symbol,
                                         std::uint64_t address) const -> bool {
  return std::ranges::any_of(
      placedClassSubobjects, [&](const ClassSubobject& placed) {
        return placed.symbol == symbol && subobjectCovers(placed, address);
      });
}

auto Binder::BuildRecordLayout::conflictsAt(const ClassSubobject& candidate,
                                            std::size_t level,
                                            std::uint64_t address) const
    -> bool {
  if (level == candidate.extents.size())
    return placedAt(candidate.symbol, address);

  const auto& extent = candidate.extents[level];
  for (std::uint64_t index = 0; index != extent.count; ++index) {
    const auto elementAddress = address + index * extent.stride;
    if (elementAddress > maxPlacedSubobjectOffset) break;
    if (conflictsAt(candidate, level + 1, elementAddress)) return true;
  }
  return false;
}

auto Binder::BuildRecordLayout::classSubobjectOffset(ClassSymbol* target,
                                                     bool tryZero,
                                                     std::uint64_t alignment)
    -> std::uint64_t {
  const auto& subobjects = binder.emptyClassSubobjects(target);

  auto conflicts = [&](std::uint64_t offset) {
    return std::ranges::any_of(
        subobjects, [&](const ClassSubobject& candidate) {
          return conflictsAt(candidate, 0, offset + candidate.offset);
        });
  };

  if (tryZero && !conflicts(0)) return 0;

  auto offset = align_to(calculatedSize, alignment);
  while (conflicts(offset)) offset += alignment;
  return offset;
}

void Binder::BuildRecordLayout::recordNonVirtualClassSubobjects(
    ClassSymbol* target, std::uint64_t offset, ClassSubobjectExtent extent) {
  const auto first = placedClassSubobjects.size();

  binder.appendClassSubobjects(placedClassSubobjects, target, offset, extent);

  for (auto index = first; index != placedClassSubobjects.size(); ++index) {
    maxPlacedSubobjectOffset = std::max(
        maxPlacedSubobjectOffset, subobjectLast(placedClassSubobjects[index]));
  }
}

void Binder::BuildRecordLayout::growSizeof(std::uint64_t offset,
                                           std::uint64_t sizeInBytes) {
  runningSizeof = std::max(runningSizeof, offset + sizeInBytes);
}

void Binder::BuildRecordLayout::recordEmptyComponent(
    std::uint64_t offset, std::uint64_t sizeInBytes) {
  growSizeof(offset, sizeInBytes);
  emptyComponentEnd = std::max(emptyComponentEnd, offset + sizeInBytes);
}

auto Binder::BuildRecordLayout::allocateBaseSubobject(ClassSymbol* base,
                                                      bool isVirtual)
    -> ClassLayout::MemberInfo {
  auto baseLayout = base->layout();

  auto baseAlignment = static_cast<int>(base->alignment());
  if (baseLayout && !baseLayout->virtualBases().empty()) {
    baseAlignment = static_cast<int>(baseLayout->nonVirtualAlignment());
  }
  baseAlignment = packAlignment(baseAlignment);

  const auto baseSizeInBytes = static_cast<int>(baseNonVirtualSize(base));
  const bool isEmpty = baseLayout && baseLayout->isAbiEmpty();

  const auto baseOffset =
      classSubobjectOffset(base, isEmpty, std::max(baseAlignment, 1));

  if (!isEmpty) padTo(baseOffset);

  ClassLayout::MemberInfo baseInfo;
  baseInfo.offset = baseOffset;
  baseInfo.index = currentIndex++;
  if (isVirtual)
    layout->setVirtualBaseInfo(base, baseInfo);
  else
    layout->setBaseInfo(base, baseInfo);

  if (isEmpty) {
    const auto emptySize = memoryLayout->sizeOf(base->type()).value_or(1);
    if (isVirtual) {
      growSizeof(baseOffset, emptySize);
    } else {
      recordEmptyComponent(baseOffset, emptySize);
    }
  } else {
    calculatedSize = std::max(calculatedSize,
                              static_cast<int>(baseOffset) + baseSizeInBytes);
    emittedEnd = std::max(emittedEnd, baseOffset + baseSizeInBytes);
  }

  recordNonVirtualClassSubobjects(base, baseOffset);
  calculatedAlignment = std::max(calculatedAlignment, baseAlignment);

  return baseInfo;
}

void Binder::BuildRecordLayout::layoutBases() {
  if (classSymbol->isUnion()) return;

  ClassSymbol* primaryBase = nullptr;
  if (!layout->primaryBaseIsVirtual()) primaryBase = layout->primaryBase();

  std::vector<ClassSymbol*> orderedBases;
  for (auto base : classSymbol->baseClasses()) {
    if (base->isVirtual()) continue;
    auto baseClassSymbol = resolved_base_class(base);
    if (!baseClassSymbol) continue;
    if (baseClassSymbol == primaryBase) {
      orderedBases.insert(orderedBases.begin(), baseClassSymbol);
    } else {
      orderedBases.push_back(baseClassSymbol);
    }
  }

  for (auto baseClassSymbol : orderedBases) {
    const auto baseInfo = allocateBaseSubobject(baseClassSymbol, false);

    if (baseClassSymbol == primaryBase) layout->setVtableIndex(baseInfo.index);
  }

  nextBitPos = calculatedSize * 8;
}

void Binder::BuildRecordLayout::layoutVirtualBases() {
  if (classSymbol->isUnion()) return;

  if (layout->primaryBaseIsVirtual()) {
    auto primary = layout->primaryBase();
    if (auto info = layout->getVirtualBaseInfo(primary)) {
      recordIndirectPrimaryPlacement(primary, {info->offset, info->index});
      recordPrimaryChain(primary, info->offset, info->index);
    }
  }

  for (auto base : classSymbol->baseClasses()) {
    if (base->isVirtual()) continue;
    auto baseClass = resolved_base_class(base);
    if (!baseClass) continue;
    if (auto info = layout->getBaseInfo(baseClass))
      collectIndirectPrimaryPlacements(baseClass, info->offset, info->index);
  }

  for (auto virtualBase : virtualBasesInInheritanceGraphOrder(classSymbol)) {
    if (auto placement = indirectPrimaryPlacement(virtualBase)) {
      layout->setVirtualBaseInfo(virtualBase, *placement);
      layout->addVirtualBase(virtualBase);
      recordNonVirtualClassSubobjects(virtualBase, placement->offset);
      recordPrimaryChain(virtualBase, placement->offset, placement->index);
      continue;
    }

    const auto baseInfo = allocateBaseSubobject(virtualBase, true);

    layout->addVirtualBase(virtualBase);
    recordPrimaryChain(virtualBase, baseInfo.offset, baseInfo.index);
  }

  nextBitPos = calculatedSize * 8;
}

void Binder::BuildRecordLayout::recordIndirectPrimaryPlacement(
    ClassSymbol* primary, ClassLayout::MemberInfo info) {
  if (indirectPrimaryPlacement(primary)) return;
  indirectPrimaryPlacements.emplace_back(primary, info);
}

auto Binder::BuildRecordLayout::indirectPrimaryPlacement(
    ClassSymbol* primary) const -> std::optional<ClassLayout::MemberInfo> {
  for (const auto& [base, info] : indirectPrimaryPlacements) {
    if (base == primary) return info;
  }
  return std::nullopt;
}

void Binder::BuildRecordLayout::recordPrimaryChain(ClassSymbol* cls,
                                                   std::uint64_t offset,
                                                   std::uint32_t topIndex) {
  while (cls) {
    auto classLayout = cls->layout();
    if (!classLayout || !classLayout->primaryBase()) return;
    auto primary = classLayout->primaryBase();
    auto info =
        classLayout->getBaseInfo(primary, classLayout->primaryBaseIsVirtual());
    if (!info) return;
    const auto primaryOffset = offset + info->offset;
    if (classLayout->primaryBaseIsVirtual())
      recordIndirectPrimaryPlacement(primary, {primaryOffset, topIndex});
    cls = primary;
    offset = primaryOffset;
  }
}

void Binder::BuildRecordLayout::collectIndirectPrimaryPlacements(
    ClassSymbol* root, std::uint64_t rootOffset, std::uint32_t rootIndex) {
  struct PlacementWork {
    ClassSymbol* classSymbol;
    std::uint64_t offset;
  };

  std::vector<PlacementWork> pending{{root, rootOffset}};
  while (!pending.empty()) {
    auto [cls, offset] = pending.back();
    pending.pop_back();
    auto classLayout = cls->layout();
    if (!classLayout) continue;
    recordPrimaryChain(cls, offset, rootIndex);
    for (auto base : cls->baseClasses() | std::views::reverse) {
      if (base->isVirtual()) continue;
      auto baseClass = resolved_base_class(base);
      if (!baseClass) continue;
      if (auto info = classLayout->getBaseInfo(baseClass))
        pending.push_back({baseClass, offset + info->offset});
    }
  }
}

void Binder::BuildRecordLayout::closeBitfieldRun() {
  if (!inBitfieldRun) return;

  calculatedSize = (nextBitPos + 7) / 8;

  auto allocUnitSizeBytes =
      static_cast<std::uint32_t>(calculatedSize - runStartByte);

  for (auto f : runFields) {
    if (auto info = layout->getFieldInfo(f)) {
      auto updated = *info;
      updated.allocUnitSizeBytes = allocUnitSizeBytes;
      layout->setFieldInfo(f, updated);
    }
  }

  runFields.clear();
  inBitfieldRun = false;
  emittedEnd = std::max(emittedEnd, static_cast<std::uint64_t>(calculatedSize));
  currentIndex++;
}

auto Binder::BuildRecordLayout::layoutBitfield(FieldSymbol* field)
    -> std::expected<bool, std::string> {
  const bool isUnion = classSymbol->isUnion();

  int bitWidth = 0;
  if (auto& bfw = field->bitFieldWidth()) {
    if (auto iv = std::get_if<ConstInt>(&*bfw)) {
      bitWidth = static_cast<int>(iv->toIntMax());
    }
  }

  if (bitWidth == 0) {
    layoutZeroWidthBitfield(field);
    return true;
  }

  auto fieldAlign = field->effectiveAlignment();
  auto fieldSizeBytes =
      static_cast<int>(memoryLayout->sizeOf(field->type()).value_or(0));
  auto fieldSizeBits = fieldSizeBytes * 8;

  if (isUnion) {
    field->setLocalOffset(0);
    field->setBitFieldOffset(0);

    ClassLayout::MemberInfo fieldInfo;
    fieldInfo.offset = 0;
    fieldInfo.index = 0;
    fieldInfo.bitOffset = 0;
    fieldInfo.bitWidth = bitWidth;
    fieldInfo.allocUnitSizeBytes = (bitWidth + 7) / 8;
    layout->setFieldInfo(field, fieldInfo);

    // Packing may shrink a union bit-field below its declared type's storage.
    // Round the occupied payload to its effective alignment so natural fields
    // retain their allocation unit while packed fields use only what they need.
    auto fieldSizeForUnion =
        align_to((bitWidth + 7) / 8, std::max(fieldAlign, 1));
    calculatedSize = std::max(calculatedSize, fieldSizeForUnion);
    calculatedAlignment = std::max(calculatedAlignment, fieldAlign);
    return true;
  }

  // An explicit alignment begins a new allocation unit. The boundary follows
  // the request even when it is below the field type's natural alignment;
  // aggregate alignment still uses fieldAlign below. A pragma pack cap may
  // reduce the requested boundary.
  if (field->explicitAlignment()) {
    closeBitfieldRun();
    auto boundary = packAlignment(field->explicitAlignment());
    nextBitPos = align_to(nextBitPos, boundary * 8);
    calculatedSize = nextBitPos / 8;
  }

  if (fieldSizeBits > 0 && keepsBitFieldInAllocationUnit(field)) {
    auto startUnit = nextBitPos / fieldSizeBits;
    auto endUnit = (nextBitPos + bitWidth - 1) / fieldSizeBits;
    if (startUnit != endUnit) {
      if (inBitfieldRun) {
        closeBitfieldRun();
      }
      nextBitPos = align_to(nextBitPos, fieldSizeBits);
      calculatedSize = nextBitPos / 8;
    }
  }

  if (!inBitfieldRun) {
    runStartByte = calculatedSize;
    nextBitPos = runStartByte * 8;
    padTo(static_cast<std::uint64_t>(runStartByte));
    runIndex = currentIndex;
    inBitfieldRun = true;
    runFields.clear();
  }

  auto bitOffsetInRun = nextBitPos - runStartByte * 8;

  field->setLocalOffset(runStartByte);
  field->setBitFieldOffset(bitOffsetInRun);

  ClassLayout::MemberInfo fieldInfo;
  fieldInfo.offset = runStartByte;
  fieldInfo.index = runIndex;
  fieldInfo.bitOffset = bitOffsetInRun;
  fieldInfo.bitWidth = bitWidth;
  layout->setFieldInfo(field, fieldInfo);

  runFields.push_back(field);
  nextBitPos += bitWidth;
  calculatedAlignment = std::max(calculatedAlignment, fieldAlign);

  return true;
}

void Binder::BuildRecordLayout::layoutZeroWidthBitfield(FieldSymbol* field) {
  if (inBitfieldRun) closeBitfieldRun();

  if (memoryLayout->zeroWidthBitFieldAlignsAggregate())
    calculatedAlignment = std::max(calculatedAlignment, field->alignment());

  if (classSymbol->isUnion()) return;
  if (!memoryLayout->sizeOf(field->type()).value_or(0)) return;

  nextBitPos = align_to(nextBitPos, field->alignment() * 8);
  calculatedSize = (nextBitPos + 7) / 8;
}

auto Binder::BuildRecordLayout::layoutRegularField(FieldSymbol* field)
    -> std::expected<bool, std::string> {
  const bool isUnion = classSymbol->isUnion();

  closeBitfieldRun();

  const bool isEmptyDataMember = binder.traits.is_zero_size_subobject(field);

  std::optional<std::size_t> size;
  if (binder.traits.is_unbounded_array(field->type())) {
    size = 0;
  } else if (isEmptyDataMember) {
    size = 0;
  } else {
    size = memoryLayout->sizeOf(field->type());
  }

  if (!size.has_value()) {
    return std::unexpected(
        std::format("size of incomplete type '{}'",
                    to_string(field->type(), field->name())));
  }

  if (isUnion) {
    field->setLocalOffset(0);
    calculatedSize = std::max(calculatedSize, int(size.value()));

    ClassLayout::MemberInfo fieldInfo;
    fieldInfo.offset = 0;
    fieldInfo.index = 0;
    layout->setFieldInfo(field, fieldInfo);
  } else {
    auto fieldAlign = field->effectiveAlignment();
    auto fieldOffset =
        static_cast<std::uint64_t>(align_to(calculatedSize, fieldAlign));
    auto elementClass = binder.fieldElementClass(field);
    if (elementClass) {
      fieldOffset = classSubobjectOffset(elementClass, isEmptyDataMember,
                                         std::max(fieldAlign, 1));
    }
    field->setLocalOffset(static_cast<int>(fieldOffset));

    if (!isEmptyDataMember) padTo(fieldOffset);

    ClassLayout::MemberInfo fieldInfo;
    fieldInfo.offset = fieldOffset;
    fieldInfo.index = currentIndex++;
    layout->setFieldInfo(field, fieldInfo);

    if (!isEmptyDataMember) {
      auto allocatedExtent = static_cast<std::uint64_t>(size.value());
      auto emittedExtent = allocatedExtent;

      if (field->isNoUniqueAddress() &&
          unqualified_cast<ClassType>(field->type())) {
        emittedExtent = binder.traits.non_virtual_size(field->type());
        allocatedExtent =
            std::max(emittedExtent, binder.traits.data_size(field->type()));
      }

      calculatedSize = std::max(
          calculatedSize, static_cast<int>(fieldOffset + allocatedExtent));
      emittedEnd = std::max(emittedEnd, fieldOffset + emittedExtent);
    }

    if (field->isNoUniqueAddress()) {
      const auto memberSize = memoryLayout->sizeOf(field->type()).value_or(0);
      if (isEmptyDataMember) {
        recordEmptyComponent(fieldOffset, memberSize);
      } else {
        growSizeof(fieldOffset, memberSize);
      }
    }

    if (elementClass) {
      recordNonVirtualClassSubobjects(elementClass, fieldOffset,
                                      binder.fieldArrayExtent(field));
    }
  }

  nextBitPos = calculatedSize * 8;

  calculatedAlignment =
      std::max(calculatedAlignment, field->effectiveAlignment());
  return true;
}

auto Binder::BuildRecordLayout::isPackedClass() const -> bool {
  return findAttribute(classSymbol->attributes(), "packed") != nullptr;
}

auto Binder::BuildRecordLayout::packAlignment(int alignment) const -> int {
  if (packValue <= 0) return alignment;
  return std::min(alignment, packValue);
}

auto Binder::BuildRecordLayout::keepsBitFieldInAllocationUnit(
    FieldSymbol* field) const -> bool {
  if (packValue > 0) return false;
  return !field->isPacked();
}

auto Binder::BuildRecordLayout::layoutFields()
    -> std::expected<bool, std::string> {
  FieldSymbol* lastField = nullptr;

  for (auto field : views::members(classSymbol) | views::non_static_fields) {
    if (lastField && binder.traits.is_unbounded_array(lastField->type())) {
      return std::unexpected(
          std::format("size of incomplete type '{}'",
                      to_string(lastField->type(), lastField->name())));
    }

    if (!field->alignment()) {
      if (isDependent(binder.unit_, field->type())) return false;
      return std::unexpected(
          std::format("alignment of incomplete type '{}'",
                      to_string(field->type(), field->name())));
    }

    if (field->isBitField()) {
      if (auto status = layoutBitfield(field); !status) return status;
    } else {
      if (auto status = layoutRegularField(field); !status) return status;
    }

    lastField = field;
  }

  closeBitfieldRun();
  return true;
}

void Binder::BuildRecordLayout::propagateBaseFields() {
  for (auto base : classSymbol->baseClasses()) {
    auto baseClassSymbol = resolved_base_class(base);
    if (!baseClassSymbol) continue;

    auto baseLayout = baseClassSymbol->layout();
    if (!baseLayout) continue;

    auto baseInfo = layout->getBaseInfo(baseClassSymbol, base->isVirtual());
    if (!baseInfo) continue;

    copyFieldInfos(baseClassSymbol, baseLayout, baseInfo->offset);
  }

  propagateAnonymousFields(classSymbol, layout.get(), 0);
}

void Binder::BuildRecordLayout::propagateAnonymousFields(
    ClassSymbol* owner, const ClassLayout* ownerLayout,
    std::uint64_t ownerOffset) {
  for (auto field : views::members(owner) | views::non_static_fields) {
    auto anonymous = anonymous_member_class(field);
    if (!anonymous) continue;

    auto anonymousLayout = anonymous->layout();
    if (!anonymousLayout) continue;

    auto fieldInfo = ownerLayout->getFieldInfo(field);
    if (!fieldInfo) continue;

    const auto anonymousOffset = ownerOffset + fieldInfo->offset;
    copyFieldInfos(anonymous, anonymousLayout, anonymousOffset);
    propagateAnonymousFields(anonymous, anonymousLayout, anonymousOffset);
  }
}

void Binder::BuildRecordLayout::copyFieldInfos(ClassSymbol* owner,
                                               const ClassLayout* ownerLayout,
                                               std::uint64_t ownerOffset) {
  for (auto field : views::members(owner) | views::non_static_fields) {
    auto info = ownerLayout->getFieldInfo(field);
    if (!info) continue;
    info->offset += ownerOffset;
    layout->setFieldInfo(field, *info);
  }
}

void Binder::BuildRecordLayout::padTo(std::uint64_t offset) {
  if (offset <= emittedEnd) return;
  layout->addPadding(currentIndex++, emittedEnd, offset - emittedEnd);
  emittedEnd = offset;
}

void Binder::BuildRecordLayout::finalize() {
  if (auto requested = classSymbol->explicitAlignment()) {
    if (requested < calculatedAlignment) {
      binder.error(
          classSymbol->location(),
          std::format("requested alignment is less than minimum "
                      "alignment of {} for type '{}'",
                      calculatedAlignment, to_string(classSymbol->type())));
    } else {
      calculatedAlignment = requested;
    }
  }

  calculatedAlignment =
      std::max(calculatedAlignment, classSymbol->minimumAlignment());

  const auto dataSize = static_cast<std::uint64_t>(calculatedSize);

  calculatedSize = std::max(calculatedSize, static_cast<int>(runningSizeof));

  if (calculatedSize == 0 && !binder.isC()) calculatedSize = 1;

  calculatedSize = align_to(calculatedSize, calculatedAlignment);

  padTo(static_cast<std::uint64_t>(calculatedSize));

  classSymbol->setAlignment(calculatedAlignment);
  classSymbol->setSizeInBytes(calculatedSize);

  layout->setSize(calculatedSize);
  layout->setAlignment(calculatedAlignment);
  layout->setDataSize(dataSize);

  binder.emptyClassSubobjects_.erase(classSymbol->resolvedDefinition());
  classSymbol->setLayout(std::move(layout));

  buildVTableLayout();
}

void Binder::BuildRecordLayout::buildVTableLayout() {
  auto classLayout = classSymbol->layout();
  if (!classLayout || !classLayout->hasVtable()) return;
  binder.buildVTableLayout(classSymbol);
}
}  // namespace cxx
