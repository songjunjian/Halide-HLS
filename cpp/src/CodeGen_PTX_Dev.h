#ifndef HALIDE_CODEGEN_PTX_DEV_H
#define HALIDE_CODEGEN_PTX_DEV_H

/** \file
 * Defines the code-generator for producing CUDA host code
 */

#include "CodeGen.h"

namespace llvm {
class BasicBlock;
}

namespace Halide { 
namespace Internal {

/** A code generator that emits GPU code from a given Halide stmt. */
class CodeGen_PTX_Dev : public CodeGen {
public:
    friend class CodeGen_PTX_Host;

    /** Create a PTX device code generator. */
    CodeGen_PTX_Dev();
        
    void compile(Stmt stmt, std::string name, const std::vector<Argument> &args);

    /** (Re)initialize the PTX module. This is separate from compile, since 
     * a PTX device module will often have many kernels compiled into it for
     * a single pipeline. */
    void init_module();

    static void test();

    static bool is_simt_var(const std::string &name);

protected:
    using CodeGen::visit;

    /** We hold onto the basic block at the start of the device
     * function in order to inject allocas */
    llvm::BasicBlock *entry_block;

    /** Nodes for which we need to override default behavior for the GPU runtime */
    // @{
    void visit(const For *);
    void visit(const Allocate *);
    void visit(const Pipeline *);
    // @}

    std::string march() const;
    std::string mcpu() const;
    std::string mattrs() const;
    bool use_soft_float_abi() const;

    std::string compile_to_ptx();

    /** Map from simt variable names (e.g. foo.blockidx) to the llvm
     * ptx intrinsic functions to call to get them. */
    std::string simt_intrinsic(const std::string &name);
};

}}

#endif
