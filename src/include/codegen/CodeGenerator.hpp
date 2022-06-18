#ifndef CODEGEN_CODE_GENERATOR_H
#define CODEGEN_CODE_GENERATOR_H

#include "sema/SymbolTable.hpp"
#include "visitor/AstNodeVisitor.hpp"

#include <memory>
#include <cstdarg>

static void dumpInstructions(FILE *p_out_file, const char *format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(p_out_file, format, args);
    va_end(args);
}

class CodeGenerator final : public AstNodeVisitor {
  private:
    const SymbolManager *m_symbol_manager_ptr;
    std::string m_source_file_path;
    std::unique_ptr<FILE> m_output_file;

  public:
    ~CodeGenerator() = default;
    CodeGenerator(const std::string source_file_name,
                  const std::string save_path,
                  const SymbolManager *const p_symbol_manager);

    void visit(ProgramNode &p_program) override;
    void visit(DeclNode &p_decl) override;
    void visit(VariableNode &p_variable) override;
    void visit(ConstantValueNode &p_constant_value) override;
    void visit(FunctionNode &p_function) override;
    void visit(CompoundStatementNode &p_compound_statement) override;
    void visit(PrintNode &p_print) override;
    void visit(BinaryOperatorNode &p_bin_op) override;
    void visit(UnaryOperatorNode &p_un_op) override;
    void visit(FunctionInvocationNode &p_func_invocation) override;
    void visit(VariableReferenceNode &p_variable_ref) override;
    void visit(AssignmentNode &p_assignment) override;
    void visit(ReadNode &p_read) override;
    void visit(IfNode &p_if) override;
    void visit(WhileNode &p_while) override;
    void visit(ForNode &p_for) override;
    void visit(ReturnNode &p_return) override;
    void initLocal(const SymbolTable *table);
    void pushVarAddr(const VariableReferenceNode &var);
    void pushVar(SymbolEntry& symbol, int size);
    void pushReg(const char* reg);
    void pop2Reg(const char* reg);
    void saveRegs();
    void loadRegs();
    void dumpInstrs(const char* format, ...) {
        va_list args;
        va_start(args, format);
        vfprintf(m_output_file.get(), format, args);
        va_end(args);
    }

};

#endif
