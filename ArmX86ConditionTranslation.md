ARM and x86 condition codes - Going from one to the other

# Introduction #
This page offers a summary of the philosophy of condition codes in the ARM and x86 cores, and the way to translate them


# Details #

There are instructions in ARM that modify flags some of the time but not
at other times. In the x86 instruction set, some instructions modify flags
all the time and some never do. This subtle difference is the philosophy
of the two instruction sets offers an interesting challenge to converting
code from one instruction set to the other.

> That a particular ARM instruction will update a flag is indicated by a bit
> named 'S' that is present in every data processing instruction. If the 'S'
> bit is '1', the flags will be updated, and if the 'S' bit is '0', they will
> not be. Instruction such as 'compare', whose sole purpose is to update flags
> are not permitted to have 'S' set to '0'.

> The challenge of conversion is interesting because in ARM programs, those
> instructions that update flags can be freely interspersed with other
> instructions that would normally update flags but are configured by the
> compiler not to do so. For instance, an 'add' should update the carry, but
> the compiler might choose it not to. This might be because the compiler
> finds that the result does not influence a decision in any case. In contrast,
> an 'add' performed on x86 would unconditionally update the carry, and
> possibly the sign and overflow flags. Clearly, a one-to-one translation
> would break the semantics of the program. Consider the case where the
> sequence of ARM instructions is as follows:

> sub [R10](https://code.google.com/p/arm-x86/source/detail?r=10), [R10](https://code.google.com/p/arm-x86/source/detail?r=10), [R10](https://code.google.com/p/arm-x86/source/detail?r=10) (encoded with S = 1)

> add [R2](https://code.google.com/p/arm-x86/source/detail?r=2), [R1](https://code.google.com/p/arm-x86/source/detail?r=1), [R0](https://code.google.com/p/arm-x86/source/detail?r=0) (encoded with S = 0)

> beq 

&lt;addr&gt;

 (branch if 0)

> Here the sub instruction changes the zero flag. The add that follows it
> normally would but does not in this case because 'S' is cleared in the
> encoding of the instruction. The beq instruction uses the result of the
> subtraction.
> A direct translation of the instructions to x86 would make the x86 condition
> check use the result of the add!

> It is observed that the trend in the gcc compiler for ARM is that unless
> an instruction produces a result that influences a conditional branch
> or conditional expression, the 'S' bit in the instruction is set to 0.
> This means that only instructions that modify flags for a if-then-else
> or instructions that modify the loop control variable in the a loop would
> have the 'S' bit set to 1. Assuming that a loop has a few instructions,
> perhaps only one instruction close to the end of the loop would modify the
> flag that controls the loop. Obviously, the ratio os instructions that
> modify the flags to those that do not, is very small. It follows that
> the strategy that offers greater speed would be one that treats instructions
> that do modify flags as special cases.

> Hence the following strategy is adopted:
> There is a flags variable that is maintained as a global variable by the
> binary translator and meant to be a snapshot of the x86 flags. When an
> instruction has the 'S' bit set to 1, this variable is loaded into the x86
> flags register using the following combination of instructions:

> push 

&lt;flags&gt;

  **Pushes the global flags variable on to the stack**

> popf          **Loads the flags register from the top of the stack**

> This is like loading the last flag context, i.e. the context created by the
> last instruction to modify the flags.
> An instruction that checks the condition and executes can then proceed
> by using native x86 condition checks. An instruction that modifies the
> flags can update the x86 flags register itself.
> Once the instruction has been completed, if the flags have been updated,
> the snapshot of the flags must once again be saved. This is done by the
> following combination of instructions:

> pushf        **Pushes the flags register to the stack**

> pop 

&lt;flags&gt;

  **Loads the flags variable from the top of the stack**

> This strategy ensures that even if there are instructions that do not
> modify flags that are placed in between instructions that do and those
> that check them (such a strategy may be adopted for optimization when the
> compiler wants to keep the pipeline full), the logic remains valid.