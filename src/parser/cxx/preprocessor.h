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

#pragma once

#include <cxx/preprocessor_fwd.h>
#include <cxx/preprocessor_snapshot.h>
#include <cxx/source_resolver.h>
#include <cxx/triple.h>

#include <array>
#include <functional>
#include <iosfwd>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace cxx {

class Token;
class DiagnosticsClient;
class CommentHandler;
class Preprocessor;
class PreprocessorDelegate;
class SourcePosition;

class CommentHandler {
 public:
  virtual ~CommentHandler() = default;

  virtual void handleComment(Preprocessor* preprocessor,
                             const Token& token) = 0;
};

// An immutable toolchain definition. Tables are sorted by identifier, with one
// definition per identifier, and outlive every preprocessor that borrows them.
struct PredefinedMacro {
  constexpr PredefinedMacro(std::string_view name, std::string_view body)
      : name(name),
        body(body),
        identifierLength_(name.substr(0, name.find('(')).size()) {}

  // Macro spelling, including the parameter list for a function-like macro.
  std::string_view name;
  // Replacement text, tokenized only when the definition is needed.
  std::string_view body;

  [[nodiscard]] constexpr auto identifier() const -> std::string_view {
    return {name.data(), identifierLength_};
  }

 private:
  // Retained identifier extent avoids rescanning declarations during lookup.
  std::size_t identifierLength_;
};

// Compile-time index of a borrowed, sorted definition table. Negative lookup
// filters depend on spelling families, not the number of definitions in them.
class PredefinedMacros {
 public:
  consteval explicit PredefinedMacros(
      std::span<const PredefinedMacro> definitions)
      : definitions_(definitions) {
    if (definitions.empty()) return;
    const auto first = definitions.front().identifier();
    const auto last = definitions.back().identifier();
    std::size_t common = 0;
    while (common < first.size() && common < last.size() &&
           first[common] == last[common]) {
      ++common;
    }
    prefix_ = first.substr(0, common);
    std::string_view previous;
    for (const auto& definition : definitions) {
      const auto name = definition.identifier();
      if (name.empty() || (!previous.empty() && previous >= name)) {
        throw "predefined macros must be sorted by unique identifiers";
      }
      previous = name;
      lengths_ |= std::uint64_t{1} << (name.size() & 63);
      if (name.size() > common) {
        const auto ch = static_cast<unsigned char>(name[common]);
        nextCharacters_[ch / 64] |= std::uint64_t{1} << (ch & 63);
      }
    }
  }

  [[nodiscard]] auto definitions() const -> std::span<const PredefinedMacro> {
    return definitions_;
  }

  [[nodiscard]] auto find(std::string_view name) const
      -> const PredefinedMacro*;

 private:
  // Borrowed definitions, sorted by identifier, with unique names.
  std::span<const PredefinedMacro> definitions_;
  // Shared identifier prefix, borrowing spelling bytes from the first row.
  std::string_view prefix_;
  // Possible identifier lengths modulo 64; false positives use exact lookup.
  std::uint64_t lengths_ = 0;
  // Possible first bytes after the common prefix.
  std::array<std::uint64_t, 4> nextCharacters_ = {};
};

class Preprocessor final : public SourceResolver {
 public:
  Preprocessor(Preprocessor&&) noexcept = default;
  auto operator=(Preprocessor&&) noexcept -> Preprocessor& = default;

  explicit Preprocessor(Control* control, DiagnosticsClient* diagnosticsClient);
  ~Preprocessor();

  [[nodiscard]] auto control() const -> Control*;

  [[nodiscard]] auto diagnosticsClient() const -> DiagnosticsClient*;

  [[nodiscard]] auto language() const -> LanguageKind;
  void setLanguage(LanguageKind lang);

  [[nodiscard]] auto targetArch() const -> TripleArch;
  void setTargetArch(TripleArch arch);

  [[nodiscard]] auto preprocessorDelegate() const -> PreprocessorDelegate*;

  [[nodiscard]] auto commentHandler() const -> CommentHandler*;
  void setCommentHandler(CommentHandler* commentHandler);

  // Returns the current directory with generic '/' separators.
  [[nodiscard]] auto currentPath() const -> std::string;
  void setCurrentPath(std::string currentPath);

  [[nodiscard]] auto omitLineMarkers() const -> bool;
  void setOmitLineMarkers(bool omitLineMarkers);

  void setOnWillIncludeHeader(
      std::function<void(const std::string&, int)> willIncludeHeader);

  void beginPreprocessing(std::string source, std::string fileName,
                          std::vector<Token>& outputTokens);

  void endPreprocessing(std::vector<Token>& outputTokens);

  static constexpr std::string_view kBuiltinsFileName = "<builtins>";

  [[nodiscard]] auto builtinsFileId() const -> int;

  [[nodiscard]] auto mainSourceFileId() const -> int;

  [[nodiscard]] auto continuePreprocessing(std::vector<Token>& outputTokens)
      -> PreprocessingState;

  [[nodiscard]] auto sourceFileName(uint32_t sourceFileId) const
      -> const std::string&;

  [[nodiscard]] auto source(uint32_t sourceFileId) const -> const std::string&;

  void preprocess(std::string source, std::string fileName,
                  std::vector<Token>& tokens);

  void getPreprocessedText(
      const std::vector<Token>& tokens,
      const std::vector<std::pair<unsigned, int>>& packChanges,
      std::ostream& out) const;

  // Returns include search directories with generic '/' separators.
  [[nodiscard]] auto systemIncludePaths() const
      -> const std::vector<std::string>&;

  void addSystemIncludePath(std::string path);

  // Returns quote-include directories with generic '/' separators.
  [[nodiscard]] auto quoteIncludePaths() const
      -> const std::vector<std::string>&;

  void addQuoteIncludePath(std::string path);

  // Returns user-include directories with generic '/' separators.
  [[nodiscard]] auto userIncludePaths() const
      -> const std::vector<std::string>&;

  void addUserIncludePath(std::string path);

  [[nodiscard]] auto includedFiles() const
      -> const std::vector<std::pair<std::string, bool>>&;

  [[nodiscard]] auto isSystemHeader(std::uint32_t sourceFileId) const -> bool;

  void setDisableCurrentDirSearch(bool disable);

  // Installs the immutable base environment before the first undefinition or
  // preprocessing operation. Explicit definitions override this environment.
  // Registration borrows the table without inspecting its rows; restore
  // replaces it with the complete environment in the snapshot.
  void setPredefinedMacros(const PredefinedMacros& macros);
  void setPredefinedMacros(PredefinedMacros&&) = delete;

  void defineMacro(const std::string& name, const std::string& body);

  void undefMacro(const std::string& name);

  void printMacros(std::ostream& out) const;

  void setPreambleOnly(bool enabled);
  [[nodiscard]] auto preambleOnly() const -> bool;
  [[nodiscard]] auto preambleSize() const -> std::optional<std::size_t>;
  [[nodiscard]] auto preambleState() const -> PreprocessorSnapshot;

  [[nodiscard]] auto canSnapshot() const -> bool;

  [[nodiscard]] auto snapshotBlocker() const -> std::string_view;

  [[nodiscard]] auto snapshot() const -> PreprocessorSnapshot;

  void restore(const PreprocessorSnapshot& snapshot);

  struct Source {
    std::string_view fileName;
    std::span<int> lineOffsets;
  };

  [[nodiscard]] auto sources() const -> std::vector<Source>;

  [[nodiscard]] auto presumedTokenStartPosition(const Token& token) const
      -> SourcePosition;

  [[nodiscard]] auto tokenStartPosition(const Token& token) const
      -> SourcePosition override;

  [[nodiscard]] auto tokenEndPosition(const Token& token) const
      -> SourcePosition override;

  [[nodiscard]] auto getTextLine(const Token& token) const
      -> std::string_view override;

  [[nodiscard]] auto getTokenText(const Token& token) const
      -> std::string_view override;

  [[nodiscard]] auto includeStack(const Token& token) const
      -> std::vector<SourcePosition> override;

  [[nodiscard]] auto resolve(const Include& include, bool isIncludeNext) const
      -> std::optional<std::string>;

  void requestCodeCompletionAt(std::uint32_t line, std::uint32_t column);

  [[nodiscard]] auto includeCompletion() const -> const IncludeCompletion*;

  [[nodiscard]] static auto isInsideIncludeDirective(std::string_view source,
                                                     std::size_t offset)
      -> bool;

  [[nodiscard]] auto hasCodeCompletionRequest() const -> bool;

  void squeeze();

 private:
  struct Private;
  struct ParseArguments;
  friend struct PendingInclude;
  friend struct PendingFileContent;
  friend struct PendingIncludeCompletion;
  friend class DefaultPreprocessorState;
  std::unique_ptr<Private> d;
};

class DefaultPreprocessorState {
  Preprocessor& self;
  bool done = false;

 public:
  explicit DefaultPreprocessorState(Preprocessor& self) : self(self) {}

  explicit operator bool() const { return !done; }

  void operator()(const ProcessingComplete&);
  void operator()(const CanContinuePreprocessing&);
  void operator()(const PendingInclude& status);
  void operator()(const PendingHasIncludes& status);
  void operator()(const PendingFileContent& status);
  void operator()(const PendingIncludeCompletion& status);
  void operator()(const EnteringFile&);
  void operator()(const LeavingFile&);
};

}  // namespace cxx
