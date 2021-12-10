#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

int token;           //current token
char *src, *old_src; //pointer to source code string
int poolsize;        //default pool size ,including text/data/stack
int line;            //line number

int *text;     //text sgment
int *old_text; // for dump text segment
int *stack;    //stack segment
char *data;    //data segment

//virtual machine registers
int *pc; //程序计数器，存放一个内存地址，该内存地址存储 下一条 即将要执行的指令
int *sp; //指针寄存器，指向栈顶
int *bp; //基址指针
int ax;  //通用寄存器，存放指令执行结果
int cycle;

// VM instructions
enum
{
    /**
     * sub_function(arg1, arg2, arg3);
     * 
     * |    ....       | high address
     * +---------------+
     * | arg: 1        |    new_bp + 4
     * +---------------+
     * | arg: 2        |    new_bp + 3
     * +---------------+
     * | arg: 3        |    new_bp + 2
     * +---------------+
     * |return address |    new_bp + 1
     * +---------------+
     * | old BP        | <- new BP
     * +---------------+
     * | local var 1   |    new_bp - 1
     * +---------------+
     * | local var 2   |    new_bp - 2
     * +---------------+
     * |    ....       |  low address
     * 
     * LEA <offset>
     * */
    LEA,
    /**
     * MOV dest, source （Intel 风格），表示将 source 的内容放在 dest 中
     * IMM <num> 将<num>放入寄存器ax中
     * ax = *pc;pc++;
     * */
    IMM,
    JMP,  // JMP <addr> 无条件跳转,将pc寄存器设置为指定的addr
    CALL, // CALL <addr> 跳转至地址为<addr>的子函数
    JZ,   // ax结果为零跳转
    JNZ,  // ax结果不为零跳转
    /**
     * enter，ENT <size> ，用于实现 ‘make new call frame’ 的功能，即保存当前的栈指针，同时在栈上保留一定的空间，用以存放局部变量：对应X86汇编代码:
     * ; make new call frame
     * push    ebp
     * mov     ebp, esp
     * sub     1, esp       ; save stack for variable: i
     * */
    ENT,
    /**
     * ADJ <size> 用于实现 ‘remove arguments from frame’。在将调用子函数时压入栈中的数据清除，本质上是因为我们的 ADD 指令功能有限。对应的汇编代码为：
     * ; remove arguments from frame
     * add     esp, 12
     * */
    ADJ,
    /** 
     * RET, 从子函数返回，此虚拟机不使用RET，用LEV替代
     * LEA对应的汇编指令为：
     * ; restore old call frame
     * mov     esp, ebp
     * pop     ebp
     * ; return
     * ret
     * */
    LEV,
    LI,   // load int 将对应地址中的整数载入ax中，要求ax中存放地址ax = *(int *)ax;
    LC,   // load char 将对应地址中的字符载入ax中，要求ax中存放地址ax = *(char *)ax;
    SI,   // send int 将ax中的数据作为整数存放到地址中，要求栈顶存放地址 *(int *)sp = ax; sp++;
    SC,   //send char 将ax中的数据作为字符存放到地址中，要求栈顶存放地址 *(char *)sp = ax; sp++;
    PUSH, //将ax的值放入栈顶
    OR,
    XOR,
    AND,
    EQ,
    NE,
    LT,
    GT,
    LE,
    GE,
    SHL,
    SHR,
    ADD,
    SUB,
    MUL,
    DIV,
    MOD,
    OPEN,
    READ,
    CLOS,
    PRTF,
    MALC,
    MSET,
    MCMP,
    EXIT
};

// tokens and classes (operators last and precedence orders)
enum
{
    Num = 128,
    Fun,
    Sys,
    Glo,
    Loc,
    Id,
    Char,
    Else,
    Enum,
    If,
    Int,
    Return,
    Sizeof,
    While,
    Assign,
    Cond,
    Lor,
    Lan,
    Or,
    Xor,
    And,
    Eq,
    Ne,
    Lt,
    Gt,
    Le,
    Ge,
    Shl,
    Shr,
    Add,
    Sub,
    Mul,
    Div,
    Mod,
    Inc,
    Dec,
    Brak
};

/**
struct identifier
{
    // * 该标识符返回的标记，理论上所有的变量返回的标记都应该是 Id，
    // * 但实际上由于我们还将在符号表中加入关键字如 if, while 等，
    // * 它们都有对应的标记。
    int token; 
    // * 标识符的哈希值，用于标识符的快速比较
    int hash;   
    char *name; //存放标识符本身的字符串
    int class;  //标识符类别，如数字，全局变量，或者局部变量等
    int type;   //标识符类型，如果是变量，变量四int、char、还是指针
    int value;  //标识符的值，如果标识符是函数，则存放地址
    // * BXXXX
    // * C 语言中标识符可以是全局的也可以是局部的，
    // * 当局部标识符的名字与全局标识符相同时，用作保存全局标识符的信息。
    int Bclass; 
    int Btype;  
    int Bvalue; 
};
// fields of identifier
*/
enum
{
    Token,
    Hash,
    Name,
    Type,
    Class,
    Value,
    BType,
    BClass,
    Bvalue,
    IdSize
};

int token_val;   //value of current token(mainly for number)
int *current_id; // current parsed ID
/**
 * Symbol table:
----+-----+----+----+----+-----+-----+-----+------+------+----
 .. |token|hash|name|type|class|value|btype|bclass|bvalue| ..
----+-----+----+----+----+-----+-----+-----+------+------+----
    |<---       one single identifier                --->|
 * */
int *symbols; // Symbol table

/*
* get next token
*/
void next()
{
    char *last_pos;
    int hash;

    while (token = *src)
    {
        src++;

        if (token == '\n')
        {
            line++;
        }
        else if (token == '#')
        {
            while (*src != 0 && *src != '\n')
            {
                src++;
            }
        }
        else if ((token >= 'a' && token <= 'z') || (token >= 'A' && token <= 'Z') || (token == '_'))
        {
            //parse identifier
            last_pos = src - 1;
            hash = token;

            while ((*src >= 'a' && *src <= 'z') || (*src >= 'A' && *src <= 'Z') || (*src >= '0' && *src <= '9') || (*src == '_'))
            {
                hash = hash * 31 + *src;
                src++;
            }

            //look for existing identifier, linear searching
            current_id = symbols;
            while (current_id[Token])
            {
                if (current_id[Hash] == hash && !memcmp((char *)current_id[Name], last_pos, src - last_pos))
                {
                    //found one, return
                    token = current_id[Token];
                    return;
                }
                current_id = current_id + IdSize;
            }

            // store new ID
            current_id[Name] = (int)last_pos;
            current_id[Hash] = hash;
            token = current_id[Token] = Id;
            return;
        }
    }

    return;
}

void expression(int level)
{
}

void program()
{
    next();
    int i_p = 0;
    while (token > 0)
    {
        i_p++;
        printf("i = %d, token is: %d\n", i_p, token);
        next();
    }
}

int eval()
{
    int op;
    int *tmp;
    cycle = 0;
    while (1)
    {
        cycle++;
        op = *pc; //获取下一条程序指令
        pc++;
        printf("LOG:op=%d,pc=%d,ax=%d,sp=%d\n", op, pc[0], ax, sp[0]);
        if (op == IMM)
        {
            ax = *pc;
            pc++;
        }
        else if (op == LC)
        {
            ax = *(char *)ax;
        }
        else if (op == LI)
        {
            ax = *(int *)ax;
        }
        else if (op == SC)
        {
            //ax = *(char *)*sp++ = ax;
            int tmpsp = *sp;
            char *castsp = (char *)tmpsp;
            *castsp = ax;
            ax = *castsp;
            sp++; //退栈
        }
        else if (op == SI)
        {
            //*(int *)*sp++ = ax;
            int tmpsp = *sp;
            int *castsp = (int *)tmpsp;
            *castsp = ax;
            sp++; //退栈
        }
        else if (op == PUSH)
        {
            sp--;
            *sp = ax;
        }
        else if (op == JMP)
        {
            pc = (int *)*pc; //pc寄存器指向的是 下一条 指令,所以此时它存放的是JMP指令的参数，即 <addr> 的值
        }
        else if (op == JZ)
        {
            pc = ax ? pc + 1 : (int *)*pc; //如果ax==0则pc跳转至(int *)*pc，否则至pc+1
        }
        else if (op == JNZ)
        {
            pc = ax ? (int *)*pc : pc + 1; //如果ax!=0则pc跳转至(int *)*pc，否则至pc+1
        }
        /**
         * int callee(int, int, int);
         * 
         * int caller(void)
         * {
         * 	    int i, ret;
         * 
         * 	    ret = callee(1, 2, 3);
         * 	    ret += 5;
         * 	    return ret;
         * }
         * 
         * * enter ENT <size>
         * ; make new call frame
         * push    ebp
         * mov     ebp, esp
         * sub     1, esp       ; save stack for variable: i
         * */
        else if (op == ENT)
        {
            sp--;
            *sp = (int)((void *)bp);
            bp = sp;
            sp = sp - *pc;
            pc++;
        }
        /**
         * LEA <size>
         * */
        else if (op == LEA)
        {
            ax = (int)(bp + *pc); //pc中存放size
            pc++;
        }
        /**
         * ; restore old call frame
         * mov     esp, ebp
         * pop     ebp
         * ; return
         * ret
         * */
        else if (op == LEV)
        {
            sp = bp;
            bp = (int *)*sp;
            sp++;
            pc = (int *)*sp;
            sp++;
        }
        /**
         * ; call subroutine 'callee'
         * call    callee
         *
         * */
        else if (op == CALL)
        {
            sp--;
            *sp = (int)(pc + 1); //从子函数中返回时，程序需要回到跳转之前的地方继续运行，在栈顶保存跳转之前的程序地址
            pc = (int *)*pc;
        }
        // else if(op == RET){
        //     pc = (int *)*sp;
        //     sp++;
        // }
        else if (op == OR)
        {
            ax = *sp | ax;
            sp++;
        }
        else if (op == XOR)
        {
            ax = *sp ^ ax;
            sp++;
        }
        else if (op == AND)
        {
            ax = *sp & ax;
            sp++;
        }
        else if (op == EQ)
        {
            ax = *sp == ax;
            sp++;
        }
        else if (op == NE)
        {
            ax = *sp != ax;
            sp++;
        }
        else if (op == LT)
        {
            ax = *sp < ax;
            sp++;
        }
        else if (op == LE)
        {
            ax = *sp <= ax;
            sp++;
        }
        else if (op == GT)
        {
            ax = *sp > ax;
            sp++;
        }
        else if (op == GE)
        {
            ax = *sp >= ax;
            sp++;
        }
        else if (op == SHL)
        {
            ax = *sp << ax;
            sp++;
        }
        else if (op == SHR)
        {
            ax = *sp >> ax;
            sp++;
        }
        else if (op == ADD)
        {
            ax = *sp + ax;
            sp++;
        }
        else if (op == SUB)
        {
            ax = *sp - ax;
            sp++;
        }
        else if (op == MUL)
        {
            ax = *sp * ax;
            sp++;
        }
        else if (op == DIV)
        {
            ax = *sp / ax;
            sp++;
        }
        else if (op == MOD)
        {
            ax = *sp % ax;
            sp++;
        }
        else if (op == EXIT)
        {
            printf("exit(%d)", *sp);
            return *sp;
        }
        else if (op == OPEN)
        {
            ax = open((char *)sp[1], sp[0]);
        }
        else if (op == CLOS)
        {
            ax = close(*sp);
        }
        else if (op == READ)
        {
            ax = read(sp[2], (char *)sp[1], *sp);
        }
        else if (op == PRTF)
        {
            tmp = sp + pc[1];
            ax = printf((char *)tmp[-1], tmp[-2], tmp[-3], tmp[-4], tmp[-5], tmp[-6]);
        }
        else if (op == MALC)
        {
            ax = (int)malloc(*sp);
        }
        else if (op == MSET)
        {
            ax = (int)memset((char *)sp[2], sp[1], *sp);
        }
        else if (op == MCMP)
        {
            ax = memcmp((char *)sp[2], (char *)sp[1], *sp);
        }
        else
        {
            printf("unknown instruction:%d\n", op);
            return -1;
        }
    }
    return 0;
}

int main(int argc, char **argv)
{
    int i;
    int fd;

    argc--;
    argv++;

    poolsize = 256 * 1024;
    line = 1;

    if (!(src = malloc(poolsize)))
    {
        printf("could not malloc(%d) for src\n", poolsize);
        return -1;
    }
    memset(src, 0, poolsize);

    // allocate memory for virtual machine
    if (!(text = old_text = malloc(poolsize)))
    {
        printf("could not malloc(%d) for text area\n", poolsize);
        return -1;
    }
    if (!(data = malloc(poolsize)))
    {
        printf("could not malloc(%d) for data area\n", poolsize);
        return -1;
    }
    if (!(stack = malloc(poolsize)))
    {
        printf("could not malloc(%d) for stack area\n", poolsize);
        return -1;
    }
    if (!(symbols = malloc(poolsize)))
    {
        printf("could not malloc(%d) for symbol table\n", poolsize);
        return -1;
    }

    //initial VM memory
    memset(text, 0, poolsize);
    memset(data, 0, poolsize);
    memset(stack, 0, poolsize);
    memset(symbols, 0, poolsize);

    //initial VM registers
    bp = sp = (int *)((int)stack + poolsize); //bp and sp at stack top at beginning
    ax = 0;

    // open source file
    if ((fd = open(*argv, 0)) < 0)
    {
        printf("could not open(%s)\n", *argv);
        return -1;
    }

    // read source file
    if ((i = read(fd, src, poolsize - 1)) <= 0)
    {
        printf("fread() return %d\n", i);
        return -1;
    }

    printf("fread() size: %d\n", i);

    src[i] = 0; // add EOF character
    close(fd);

    i = 0;
    text[i++] = IMM;
    text[i++] = 10;
    text[i++] = PUSH;
    text[i++] = IMM;
    text[i++] = 20;
    text[i++] = ADD;
    text[i++] = PUSH;
    text[i++] = EXIT;
    pc = text;

    program();
    return eval();
}