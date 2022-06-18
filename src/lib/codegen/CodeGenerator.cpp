#include "codegen/CodeGenerator.hpp"
#include "visitor/AstNodeInclude.hpp"

#include <algorithm>
#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <string>
#include <iostream>

using namespace std;


CodeGenerator::CodeGenerator(const std::string source_file_name,
                             const std::string save_path,
                             const SymbolManager *const p_symbol_manager)
    : m_symbol_manager_ptr(p_symbol_manager),
      m_source_file_path(source_file_name) {
    // FIXME: assume that the source file is always xxxx.p
    const std::string &real_path =
        (save_path == "") ? std::string{"."} : save_path;
    auto slash_pos = source_file_name.rfind("/");
    auto dot_pos = source_file_name.rfind(".");

    if (slash_pos != std::string::npos) {
        ++slash_pos;
    } else {
        slash_pos = 0;
    }
    std::string output_file_path(
        real_path + "/" +
        source_file_name.substr(slash_pos, dot_pos - slash_pos) + ".S");
    m_output_file.reset(fopen(output_file_path.c_str(), "w"));
    assert(m_output_file.get() && "Failed to open output file");
}

const char* prologue = 
    "    addi sp, sp, -128\n"
    "    sw ra, 124(sp)\n"
    "    sw s0, 120(sp)\n"
    "    addi s0, sp, 128\n";

const char* epilogue = 
    "    lw ra, 124(sp)\n"
    "    lw s0, 120(sp)\n"
    "    addi sp, sp, 128\n"
    "    jr ra\n";

const char* argRegs[] = {
        "a0",
        "a1",
        "a2",
        "a3",
        "a4",
        "a5",
        "a6",
        "a7",
        "t3",
        "t4",
        "t5",
        "t6",
    };

int stkptr = -8;

void CodeGenerator::pushVarAddr(const VariableReferenceNode &var) {
    auto *entry = m_symbol_manager_ptr -> lookup(var.getName());
    dumpInstrs("// push %s\n", entry -> getName().c_str());
	if(entry -> getLevel() == 0) {
	    dumpInstrs("    la t0, %s\n", entry -> getName().c_str());
    } else if(entry -> getLevel() != 0) {
		dumpInstrs("    addi t0, s0, %d\n", entry -> stkLoc);
    }
	pushReg("t0");
}

void CodeGenerator::pushVar(SymbolEntry& symbol, int size) {
    stkptr -= size;
    symbol.stkLoc = stkptr;
}

void CodeGenerator::pushReg(const char* reg) {
    dumpInstrs("// push %s\n", reg);
    dumpInstrs("    addi sp, sp, -4\n");
    dumpInstrs("    sw %s, 0(sp)\n", reg);
}

void CodeGenerator::pop2Reg(const char* reg) {
    dumpInstrs("// pop to %s\n", reg);
    dumpInstrs("    lw %s, 0(sp)\n", reg);
    dumpInstrs("    addi sp, sp, 4\n");
}


void CodeGenerator::initLocal(const SymbolTable *table) {

    stkptr = -8;
    int cnt = 0;

    for (const auto &ptr : table -> getEntries()) {
        auto type = ptr -> getTypePtr();
        int space;

        if (type -> isPrimitiveInteger() || type -> isPrimitiveBool()) 
            space = 4;
        
        pushVar(*ptr, space);	
        
        if(ptr -> getKind() == SymbolEntry::KindEnum::kConstantKind) {
            dumpInstrs("// local constant\n");
            dumpInstrs("    li t0, %d\n", ptr -> getAttribute().constant() -> integer());
            dumpInstrs("    sw t0, %d(s0)\n", ptr -> stkLoc);
        } else if(ptr -> getKind() == SymbolEntry::KindEnum::kParameterKind) {
            dumpInstrs("// passing parameters\n");
            dumpInstrs("    sw %s, %d(s0)\n", argRegs[cnt++], ptr -> stkLoc);
        }
    }
}

void CodeGenerator::visit(ProgramNode &p_program) {
    // Generate RISC-V instructions for program header
    // clang-format off
    const char* riscv_assembly_file_prologue =
        "    .file \"%s\"\n"
        "    .option nopic\n";

	const char* mainPrologue = 
        ".section    .text\n"
        "    .align 2\n"
        "    .globl main\n"
        "    .type main, @function\n"
        "main:\n";
    // clang-format on
    dumpInstructions(m_output_file.get(), riscv_assembly_file_prologue,
                     m_source_file_path.c_str());

    // Reconstruct the hash table for looking up the symbol entry
    // Hint: Use symbol_manager->lookup(symbol_name) to get the symbol entry.
    m_symbol_manager_ptr->reconstructHashTableFromSymbolTable(
        p_program.getSymbolTable());

    auto visit_ast_node = [&](auto &ast_node) { ast_node->accept(*this); };

    for (const auto &ptr : p_program.getSymbolTable() -> getEntries()) {
		const auto &symbol = *ptr;
        if(ptr -> getKind() == SymbolEntry::KindEnum::kVariableKind)
			dumpInstrs(".comm %s, 4, 4\n", symbol.getName().c_str());
        else if(ptr -> getKind() == SymbolEntry::KindEnum::kConstantKind){
			const Constant &cnst = *symbol.getAttribute().constant();
			dumpInstrs(".section    .rodata\n"
					   "    .align 2\n"
					   "    .globl %s\n"
					   "    .type %s, @object\n"
                       "%s:\n"
                       "    .word %d\n", 
						ptr -> getName().c_str(), 
						ptr -> getName().c_str(), 
						ptr -> getName().c_str(), cnst.integer());
		}
    }

    for_each(p_program.getDeclNodes().begin(), p_program.getDeclNodes().end(),
             visit_ast_node);
    for_each(p_program.getFuncNodes().begin(), p_program.getFuncNodes().end(),
             visit_ast_node);

    dumpInstructions(m_output_file.get(), mainPrologue);    
	dumpInstructions(m_output_file.get(), prologue);
    const_cast<CompoundStatementNode &>(p_program.getBody()).accept(*this);
    dumpInstructions(m_output_file.get(), epilogue);


    // Remove the entries in the hash table
    m_symbol_manager_ptr->removeSymbolsFromHashTable(p_program.getSymbolTable());
}

void CodeGenerator::visit(DeclNode &p_decl) {
    p_decl.visitChildNodes(*this);
}

void CodeGenerator::visit(VariableNode &p_variable) {
}

void CodeGenerator::visit(ConstantValueNode &p_constant_value) {
    auto type = p_constant_value.getTypePtr();
    auto cnst = p_constant_value.getConstantPtr();

    if(type -> isPrimitiveInteger()) {
        dumpInstrs("    li t0, %s\n", p_constant_value.getConstantValueCString());
        pushReg("t0");
    }
}

void CodeGenerator::visit(FunctionNode &p_function) {
    // Reconstruct the hash table for looking up the symbol entry
    m_symbol_manager_ptr->reconstructHashTableFromSymbolTable(
        p_function.getSymbolTable());

	dumpInstrs(".section    .text\n");
	dumpInstrs("    .align 2\n");
	dumpInstrs("    .type %s, @function\n", p_function.getName().c_str());
	dumpInstrs("%s:\n", p_function.getName().c_str());
    
    dumpInstructions(m_output_file.get(), prologue);
    initLocal(p_function.getSymbolTable());
    p_function.visitChildNodes(*this);
    dumpInstructions(m_output_file.get(), epilogue);

    // Remove the entries in the hash table
    m_symbol_manager_ptr->removeSymbolsFromHashTable(p_function.getSymbolTable());
}

void CodeGenerator::visit(CompoundStatementNode &p_compound_statement) {
    // Reconstruct the hash table for looking up the symbol entry
    m_symbol_manager_ptr->reconstructHashTableFromSymbolTable(
        p_compound_statement.getSymbolTable());


	if(p_compound_statement.getSymbolTable() != NULL)
        initLocal(p_compound_statement.getSymbolTable());

    p_compound_statement.visitChildNodes(*this);

    // Remove the entries in the hash table
    m_symbol_manager_ptr->removeSymbolsFromHashTable(
        p_compound_statement.getSymbolTable());
}

void CodeGenerator::visit(PrintNode &p_print) {
    dumpInstrs("// print\n");
    p_print.visitChildNodes(*this);
    pop2Reg("a0");
    // dumpInstrs("    lw a0, 0(t0)\n");
    dumpInstrs("    jal ra, printInt\n");
}

void CodeGenerator::visit(BinaryOperatorNode &p_bin_op) {

    p_bin_op.getL() -> accept(*this);
    p_bin_op.getR() -> accept(*this);

    pop2Reg("t0");
    pop2Reg("t1");

	switch (p_bin_op.getOp()) {
        case Operator::kPlusOp:
            dumpInstrs("// t0 = t0 + t1\n");
            dumpInstrs("    add t0, t0, t1\n");
            break;
        case Operator::kMultiplyOp:
            dumpInstrs("    mul t0, t0, t1\n");
            break;
    }
    pushReg("t0");
}

void CodeGenerator::visit(UnaryOperatorNode &p_un_op) {}

void CodeGenerator::visit(FunctionInvocationNode &p_func_invocation) {
    auto &args = p_func_invocation.getArguments();
    for (int i = 0; i < args.size(); i++) {
        dumpInstrs("//// %dth arg\n", i);
        auto &arg = *args[i];
        arg.accept(*this);
    }
    for(int i = args.size() - 1; i >= 0; i--)
        pop2Reg(argRegs[i]);
    dumpInstrs("// Calling %s\n", p_func_invocation.getNameCString());
    dumpInstrs("    jal ra, %s\n", p_func_invocation.getNameCString());
    pushReg("a0");
}

void CodeGenerator::visit(VariableReferenceNode &p_variable_ref) {
    pushVarAddr(p_variable_ref);
    pop2Reg("t0");
    dumpInstrs("    lw t0, 0(t0)\n");
    pushReg("t0");
}

void CodeGenerator::visit(AssignmentNode &p_assignment) {
    dumpInstrs("// assignment\n");
    VariableReferenceNode *lvalue = p_assignment.getL();
    pushVarAddr(*lvalue);
    p_assignment.getR() -> accept(*this);
    pop2Reg("t1"); // pop the value
    pop2Reg("t0"); // pop the address
    dumpInstrs("// *t0 = t1; \n");
    dumpInstrs("    sw t1, 0(t0)\n");
}

void CodeGenerator::visit(ReadNode &p_read) {}

void CodeGenerator::visit(IfNode &p_if) {}

void CodeGenerator::visit(WhileNode &p_while) {}

void CodeGenerator::visit(ForNode &p_for) {
    // Reconstruct the hash table for looking up the symbol entry
    m_symbol_manager_ptr->reconstructHashTableFromSymbolTable(
        p_for.getSymbolTable());

    p_for.visitChildNodes(*this);

    // Remove the entries in the hash table
    m_symbol_manager_ptr->removeSymbolsFromHashTable(p_for.getSymbolTable());
}

void CodeGenerator::visit(ReturnNode &p_return) {
    dumpInstrs("// return from stack\n");
    p_return.getRetVal() -> accept(*this);
    pop2Reg("a0");
}
