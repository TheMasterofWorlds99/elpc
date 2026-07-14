/*
   DFA.HPP
   NFA-simulation lexer engine.

   Each rule pattern is compiled to a fragment of NFA states. All fragments
   share a common start state. Tokenization simulates all active NFA states
   in parallel using a bitset — O(n × s) time where s is the number of
   states, which is typically very small.

   Correctly handles pattern collisions (e.g., keywords and identifiers
   sharing a prefix) because all paths are followed simultaneously.
*/

#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace elpc {

// -----------------------------------------------------------------------
// Character class — 256-bit bitset
// -----------------------------------------------------------------------

using CharClass = std::array<bool, 256>;

namespace detail {

inline CharClass ccNone() { CharClass cc{}; return cc; }

inline CharClass ccRange(uint8_t lo, uint8_t hi) {
  CharClass cc{};
  for (int c = lo; c <= hi; ++c) cc[c] = true;
  return cc;
}

inline CharClass ccChars(std::string_view s) {
  CharClass cc{};
  for (auto c : s) cc[static_cast<uint8_t>(c)] = true;
  return cc;
}

inline CharClass ccUnion(const CharClass &a, const CharClass &b) {
  CharClass out{};
  for (int i = 0; i < 256; ++i) out[i] = a[i] || b[i];
  return out;
}

inline CharClass ccNegate(const CharClass &in) {
  CharClass out{};
  for (int i = 0; i < 256; ++i) out[i] = !in[i];
  return out;
}

// -----------------------------------------------------------------------
// Pattern parsing — regex string → PatternStep vector
// -----------------------------------------------------------------------

struct PatternStep {
  enum Type : uint8_t { LITERAL, CHAR_CLASS, ONE_OR_MORE, ZERO_OR_MORE };
  Type type;
  union { uint8_t byteVal; int charClassIdx; };
};

class PatternParser {
  std::string_view pat;
  size_t pos = 0;
  std::vector<CharClass> &charClasses;

  char peek() const { return pos < pat.size() ? pat[pos] : '\0'; }
  char advance() { return pos < pat.size() ? pat[pos++] : '\0'; }
  bool match(char c) { if (peek() == c) { advance(); return true; } return false; }

  CharClass parseEscape() {
    char esc = advance();
    if (esc == 'b') return ccNone();
    if (esc == 'd') return ccRange('0', '9');
    if (esc == 'w') return ccUnion(ccUnion(ccRange('0', '9'), ccRange('A', 'Z')),
                                   ccUnion(ccRange('a', 'z'), ccChars("_")));
    if (esc == 's') return ccChars(" \t\n\r\f\v");
    if (esc == 'D') return ccNegate(ccRange('0', '9'));
    if (esc == 'W') return ccNegate(ccUnion(ccUnion(ccRange('0', '9'), ccRange('A', 'Z')),
                                             ccUnion(ccRange('a', 'z'), ccChars("_"))));
    if (esc == 'S') return ccNegate(ccChars(" \t\n\r\f\v"));
    if (esc == 'n') return ccChars(std::string_view("\n", 1));
    if (esc == 't') return ccChars(std::string_view("\t", 1));
    if (esc == 'r') return ccChars(std::string_view("\r", 1));
    if (esc == 'f') return ccChars(std::string_view("\f", 1));
    if (esc == 'v') return ccChars(std::string_view("\v", 1));
    if (esc == '0') return ccChars(std::string_view("\n", 0));
    return ccChars(std::string_view(&esc, 1));
  }

  int addCharClass(CharClass cc) {
    int idx = static_cast<int>(charClasses.size());
    charClasses.push_back(std::move(cc));
    return idx;
  }

  int parseCharClass() {
    bool negated = match('^');
    CharClass cc{};
    while (peek() && peek() != ']') {
      if (peek() == '\\') { advance(); cc = ccUnion(cc, parseEscape()); }
      else {
        char c1 = advance();
        if (peek() == '-' && pos + 1 < pat.size() && pat[pos + 1] != ']') {
          advance(); char c2 = advance();
          cc = ccUnion(cc, ccRange(static_cast<uint8_t>(c1), static_cast<uint8_t>(c2)));
        } else { cc = ccUnion(cc, ccChars(std::string_view(&c1, 1))); }
      }
    }
    match(']');
    return addCharClass(negated ? ccNegate(cc) : cc);
  }

  void parseAtom(std::vector<PatternStep> &steps) {
    if (match('(')) { /* group — treat as concat inside */ }
    else if (match('[')) {
      int ccIdx = parseCharClass();
      applyQuantifier(steps, {PatternStep::CHAR_CLASS, {.charClassIdx = ccIdx}});
    } else if (peek() == '\\') {
      advance(); CharClass cc = parseEscape();
      if (cc == ccNone()) return;
      int ccIdx = addCharClass(std::move(cc));
      applyQuantifier(steps, {PatternStep::CHAR_CLASS, {.charClassIdx = ccIdx}});
    } else if (peek() == '.') {
      advance();
      applyQuantifier(steps, {PatternStep::CHAR_CLASS, {.charClassIdx = addCharClass(ccNegate(ccNone()))}});
    } else {
      uint8_t c = static_cast<uint8_t>(advance());
      applyQuantifier(steps, {PatternStep::LITERAL, {.byteVal = c}});
    }
  }

  void applyQuantifier(std::vector<PatternStep> &steps, PatternStep step) {
    if (peek() == '*') { advance();
      if (step.type == PatternStep::CHAR_CLASS) step.type = PatternStep::ZERO_OR_MORE;
      steps.push_back(step);
    } else if (peek() == '+') { advance();
      if (step.type == PatternStep::CHAR_CLASS) step.type = PatternStep::ONE_OR_MORE;
      steps.push_back(step);
    } else { steps.push_back(step); }
  }

  void parseConcat(std::vector<PatternStep> &steps) {
    while (peek() && peek() != ')' && peek() != '|') parseAtom(steps);
  }

public:
  PatternParser(std::string_view pattern, std::vector<CharClass> &cc)
    : pat(pattern), charClasses(cc) {}

  std::vector<PatternStep> parse() {
    std::vector<PatternStep> steps;
    parseConcat(steps);
    return steps;
  }
};

// -----------------------------------------------------------------------
// NFA — a set of states with character-class transitions
// -----------------------------------------------------------------------

struct NFATransition {
  int charClassIdx;  // index into the charClasses table
  int target;        // state index
};

struct NFAState {
  std::vector<NFATransition> transitions;
  int acceptRule = -1;
};

struct NFA {
  std::vector<NFAState> states;
  std::vector<CharClass> charClasses;

  int newState() {
    int id = static_cast<int>(states.size());
    states.emplace_back();
    return id;
  }

  void addTransition(int from, int ccIdx, int to) {
    states[from].transitions.push_back({ccIdx, to});
  }

  void addPattern(int startState, const std::vector<PatternStep> &steps, int ruleIdx) {
    int current = startState;

    for (size_t si = 0; si < steps.size(); ++si) {
      auto &step = steps[si];
      bool isLast = (si == steps.size() - 1);

      switch (step.type) {
        case PatternStep::LITERAL: {
          int ccIdx = static_cast<int>(charClasses.size());
          CharClass cc{};
          cc[step.byteVal] = true;
          charClasses.push_back(cc);
          int next = newState();
          addTransition(current, ccIdx, next);
          current = next;
          break;
        }
        case PatternStep::CHAR_CLASS: {
          int next = newState();
          addTransition(current, step.charClassIdx, next);
          current = next;
          break;
        }
        case PatternStep::ONE_OR_MORE: {
          // Match one or more of the class
          int loop = newState();
          addTransition(current, step.charClassIdx, loop);
          addTransition(loop, step.charClassIdx, loop);  // self-loop
          current = loop;
          break;
        }
        case PatternStep::ZERO_OR_MORE: {
          // Match zero or more of the class
          // current already accepts (zero matches)
          if (isLast && states[current].acceptRule == -1)
            states[current].acceptRule = ruleIdx;
          int loop = newState();
          addTransition(current, step.charClassIdx, loop);
          addTransition(loop, step.charClassIdx, loop);  // self-loop
          if (isLast && states[loop].acceptRule == -1)
            states[loop].acceptRule = ruleIdx;
          current = loop;
          break;
        }
      }
    }

    if (states[current].acceptRule == -1)
      states[current].acceptRule = ruleIdx;
  }

  /// Compile all rules into the NFA.
  void compile(const std::vector<std::pair<std::string_view, int>> &rules) {
    states.clear();
    charClasses.clear();
    int start = newState();  // state 0 = common start

    for (auto &[pattern, ruleIdx] : rules) {
      PatternParser parser(pattern, charClasses);
      auto steps = parser.parse();
      if (steps.empty()) continue;
      addPattern(start, steps, ruleIdx);
    }
  }

  /// Run the NFA over input using parallel state simulation.
  /// Returns (matchLength, ruleIndex). matchLength = 0 means no match.
  void run(std::string_view input, size_t &outMatchLen, int &outRule) const {
    outMatchLen = 0;
    outRule = -1;

    if (states.empty()) return;

    // Active states as a simple vector (size is typically < 50)
    std::vector<bool> active(states.size(), false);
    std::vector<bool> next(states.size(), false);

    // Start with state 0
    active[0] = true;

    // Check initial accept
    if (states[0].acceptRule != -1) {
      outRule = states[0].acceptRule;
      outMatchLen = 0;
    }

    for (size_t pos = 0; pos < input.size(); ++pos) {
      uint8_t byteVal = static_cast<uint8_t>(input[pos]);

      // Compute next active set
      std::fill(next.begin(), next.end(), false);
      bool anyActive = false;

      for (int si = 0; si < static_cast<int>(states.size()); ++si) {
        if (!active[si]) continue;
        for (auto &t : states[si].transitions) {
          if (t.charClassIdx >= 0 && t.charClassIdx < static_cast<int>(charClasses.size())
              && charClasses[t.charClassIdx][byteVal]) {
            next[t.target] = true;
            anyActive = true;
          }
        }
      }

      if (!anyActive) break;  // dead — all paths exhausted

      // Swap active/next
      active.swap(next);

      // Check accepting states — longest match wins, ties broken by priority
      for (int si = 0; si < static_cast<int>(states.size()); ++si) {
        if (active[si] && states[si].acceptRule != -1) {
          if (outRule == -1 || pos + 1 > outMatchLen ||
              (pos + 1 == outMatchLen && states[si].acceptRule < outRule)) {
            outRule = states[si].acceptRule;
            outMatchLen = pos + 1;
          }
        }
      }
    }
  }
};

} // namespace detail

} // namespace elpc
