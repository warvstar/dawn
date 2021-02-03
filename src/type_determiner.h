// Copyright 2020 The Tint Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef SRC_TYPE_DETERMINER_H_
#define SRC_TYPE_DETERMINER_H_

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "src/ast/module.h"
#include "src/diagnostic/diagnostic.h"
#include "src/program_builder.h"
#include "src/scope_stack.h"
#include "src/type/storage_texture_type.h"

namespace tint {
namespace ast {

class ArrayAccessorExpression;
class BinaryExpression;
class BitcastExpression;
class CallExpression;
class ConstructorExpression;
class Function;
class IdentifierExpression;
class MemberAccessorExpression;
class UnaryOpExpression;
class Variable;

}  // namespace ast

/// Determines types for all items in the given tint program
class TypeDeterminer {
 public:
  /// Constructor
  /// @param builder the program builder
  explicit TypeDeterminer(ProgramBuilder* builder);

  /// Destructor
  ~TypeDeterminer();

  /// Run the type determiner on `program`, replacing the Program with a new
  /// program containing type information.
  /// [TEMPORARY] - Exists for making incremental changes.
  /// @param program a pointer to the program variable that will be read from
  /// and assigned to.
  /// @returns a list of diagnostic messages
  static diag::List Run(Program* program);

  /// @returns error messages from the type determiner
  const std::string& error() { return error_; }

  /// @returns true if the type determiner was successful
  bool Determine();

  /// @param name the function name to try and match as an intrinsic.
  /// @return the semantic::Intrinsic for the given name. If `name` does not
  /// match an intrinsic, returns semantic::Intrinsic::kNone
  static semantic::Intrinsic MatchIntrinsic(const std::string& name);

 private:
  template <typename T>
  struct UniqueVector {
    using ConstIterator = typename std::vector<T>::const_iterator;

    void Add(const T& val) {
      if (set.count(val) == 0) {
        vector.emplace_back(val);
        set.emplace(val);
      }
    }
    size_t size() const { return vector.size(); }
    ConstIterator begin() const { return vector.begin(); }
    ConstIterator end() const { return vector.end(); }
    operator const std::vector<T> &() const { return vector; }

   private:
    std::vector<T> vector;
    std::unordered_set<T> set;
  };

  /// Structure holding semantic information about a function.
  /// Used to build the semantic::Function nodes at the end of resolving.
  struct VariableInfo {
    explicit VariableInfo(ast::Variable* decl);
    ~VariableInfo();

    ast::Variable* const declaration;
    ast::StorageClass storage_class;
  };

  /// Structure holding semantic information about a function.
  /// Used to build the semantic::Function nodes at the end of resolving.
  struct FunctionInfo {
    explicit FunctionInfo(ast::Function* decl);
    ~FunctionInfo();

    ast::Function* const declaration;
    UniqueVector<VariableInfo*> referenced_module_vars;
    UniqueVector<VariableInfo*> local_referenced_module_vars;
    UniqueVector<Symbol> ancestor_entry_points;
  };

  /// Determines type information for the program, without creating final the
  /// semantic nodes.
  /// @returns true if the determination was successful
  bool DetermineInternal();

  /// Determines type information for functions
  /// @param funcs the functions to check
  /// @returns true if the determination was successful
  bool DetermineFunctions(const ast::FunctionList& funcs);
  /// Determines type information for a function
  /// @param func the function to check
  /// @returns true if the determination was successful
  bool DetermineFunction(ast::Function* func);
  /// Determines type information for a set of statements
  /// @param stmts the statements to check
  /// @returns true if the determination was successful
  bool DetermineStatements(const ast::BlockStatement* stmts);
  /// Determines type information for a statement
  /// @param stmt the statement to check
  /// @returns true if the determination was successful
  bool DetermineResultType(ast::Statement* stmt);
  /// Determines type information for an expression list
  /// @param list the expression list to check
  /// @returns true if the determination was successful
  bool DetermineResultType(const ast::ExpressionList& list);
  /// Determines type information for an expression
  /// @param expr the expression to check
  /// @returns true if the determination was successful
  bool DetermineResultType(ast::Expression* expr);
  /// Determines the storage class for variables. This assumes that it is only
  /// called for things in function scope, not module scope.
  /// @param stmt the statement to check
  /// @returns false on error
  bool DetermineVariableStorageClass(ast::Statement* stmt);

  /// Creates the nodes and adds them to the semantic::Info mappings of the
  /// ProgramBuilder.
  void CreateSemanticNodes() const;

  /// Retrieves information for the requested import.
  /// @param src the source of the import
  /// @param path the import path
  /// @param name the method name to get information on
  /// @param params the parameters to the method call
  /// @param id out parameter for the external call ID. Must not be a nullptr.
  /// @returns the return type of `name` in `path` or nullptr on error.
  type::Type* GetImportData(const Source& src,
                            const std::string& path,
                            const std::string& name,
                            const ast::ExpressionList& params,
                            uint32_t* id);

  void set_error(const Source& src, const std::string& msg);
  void set_referenced_from_function_if_needed(VariableInfo* var, bool local);
  void set_entry_points(const Symbol& fn_sym, Symbol ep_sym);

  bool DetermineArrayAccessor(ast::ArrayAccessorExpression* expr);
  bool DetermineBinary(ast::BinaryExpression* expr);
  bool DetermineBitcast(ast::BitcastExpression* expr);
  bool DetermineCall(ast::CallExpression* expr);
  bool DetermineConstructor(ast::ConstructorExpression* expr);
  bool DetermineIdentifier(ast::IdentifierExpression* expr);
  bool DetermineIntrinsicCall(ast::CallExpression* call,
                              semantic::Intrinsic intrinsic);
  bool DetermineMemberAccessor(ast::MemberAccessorExpression* expr);
  bool DetermineUnaryOp(ast::UnaryOpExpression* expr);

  VariableInfo* CreateVariableInfo(ast::Variable*);

  /// @returns the resolved type of the ast::Expression `expr`
  /// @param expr the expression
  type::Type* TypeOf(ast::Expression* expr) const {
    return builder_->TypeOf(expr);
  }

  /// Creates a semantic::Expression node with the resolved type `type`, and
  /// assigns this semantic node to the expression `expr`.
  /// @param expr the expression
  /// @param type the resolved type
  void SetType(ast::Expression* expr, type::Type* type) const;

  ProgramBuilder* builder_;
  std::string error_;
  ScopeStack<VariableInfo*> variable_stack_;
  std::unordered_map<Symbol, FunctionInfo*> symbol_to_function_;
  std::unordered_map<ast::Function*, FunctionInfo*> function_to_info_;
  std::unordered_map<ast::Variable*, VariableInfo*> variable_to_info_;
  FunctionInfo* current_function_ = nullptr;
  BlockAllocator<VariableInfo> variable_infos_;
  BlockAllocator<FunctionInfo> function_infos_;

  // Map from caller functions to callee functions.
  std::unordered_map<Symbol, std::vector<Symbol>> caller_to_callee_;
};

}  // namespace tint

#endif  // SRC_TYPE_DETERMINER_H_
