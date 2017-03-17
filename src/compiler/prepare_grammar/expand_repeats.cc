#include "compiler/prepare_grammar/expand_repeats.h"
#include <vector>
#include <string>
#include <utility>
#include "compiler/grammar.h"
#include "compiler/rule.h"

namespace tree_sitter {
namespace prepare_grammar {

using std::string;
using std::vector;
using std::pair;
using std::to_string;
using std::make_shared;
using rules::Blank;
using rules::Choice;
using rules::Repeat;
using rules::Seq;
using rules::Symbol;
using rules::Rule;

class ExpandRepeats {
  string rule_name;
  size_t offset;
  size_t repeat_count;
  vector<pair<Rule, Symbol>> existing_repeats;

  Rule apply(Rule rule) {
    return rule.match(
      [&](const rules::Blank &blank) -> Rule { return blank; },
      [&](const rules::Symbol &symbol) { return symbol; },

      [&](const rules::Choice &choice) {
        vector<rules::Rule> elements;
        for (const auto &element : choice.elements) {
          elements.push_back(apply(element));
        }
        return rules::Choice::build(elements);
      },

      [&](const rules::Seq &sequence) {
        return rules::Seq{
          apply(sequence.left),
          apply(sequence.right)
        };
      },

      [&](const rules::Repeat &repeat) {
        for (const auto pair : existing_repeats) {
          if (pair.first == rule) {
            return pair.second;
          }
        }

        Rule inner_rule = apply(repeat.rule);
        size_t index = aux_rules.size();
        string helper_rule_name = rule_name + "_repeat" + to_string(++repeat_count);
        Symbol repeat_symbol = Symbol::non_terminal(offset + index);
        existing_repeats.push_back({repeat, repeat_symbol});
        aux_rules.push_back({
          helper_rule_name,
          VariableTypeAuxiliary,
          Choice{{
            Seq{repeat_symbol, inner_rule},
            inner_rule,
          }}
        });
        return repeat_symbol;
      },

      [&](const rules::Metadata &metadata) {
        return rules::Metadata{apply(metadata.rule), metadata.params};
      },

      [](auto) {
        assert(false);
        return Blank{};
      }
    );
  }

 public:
  explicit ExpandRepeats(size_t offset) : offset(offset) {}

  Rule expand(const Rule &rule, const string &name) {
    rule_name = name;
    repeat_count = 0;
    return apply(rule);
  }

  vector<InitialSyntaxGrammar::Variable> aux_rules;
};

InitialSyntaxGrammar expand_repeats(const InitialSyntaxGrammar &grammar) {
  InitialSyntaxGrammar result;
  result.variables = grammar.variables;
  result.extra_tokens = grammar.extra_tokens;
  result.expected_conflicts = grammar.expected_conflicts;
  result.external_tokens = grammar.external_tokens;

  ExpandRepeats expander(result.variables.size());
  for (auto &variable : result.variables) {
    variable.rule = expander.expand(variable.rule, variable.name);
  }

  result.variables.insert(
    result.variables.end(),
    expander.aux_rules.begin(),
    expander.aux_rules.end()
  );

  return result;
}

}  // namespace prepare_grammar
}  // namespace tree_sitter
