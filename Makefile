all: main

#a3.out a2-2test.out a2test.out a1test.out

CC = g++ -O0 -Wno-deprecated

tag = -i

ifdef linux
tag = -n
endif

test_new: yyold.tab.o lex.yyold.o y.tab.o lex.yy.o test_new.o Statistics.o Optimizer.o Record.o Schema.o Function.o Comparison.o File.o EventLogger.o FileUtil.o Heap.o Sorted.o DBFile.o Pipe.o BigQ.o RelOp.o ComparisonEngine.o DDL_DML.o QueryPlan.o
	$(CC) -o test_new yyold.tab.o lex.yyold.o y.tab.o lex.yy.o Statistics.o Optimizer.o test_new.o Record.o Schema.o Function.o Comparison.o File.o EventLogger.o FileUtil.o Heap.o Sorted.o DBFile.o Pipe.o BigQ.o RelOp.o ComparisonEngine.o DDL_DML.o QueryPlan.o  -lfl -lpthread


main: yyold.tab.o lex.yyold.o y.tab.o lex.yy.o main.o Statistics.o Record.o Schema.o Function.o Comparison.o File.o EventLogger.o FileUtil.o Heap.o Sorted.o DBFile.o Pipe.o BigQ.o RelOp.o ComparisonEngine.o DDL_DML.o QueryPlan.o
	$(CC) -o main yyold.tab.o lex.yyold.o y.tab.o lex.yy.o Statistics.o main.o Record.o Schema.o Function.o Comparison.o File.o EventLogger.o FileUtil.o Heap.o Sorted.o DBFile.o Pipe.o BigQ.o RelOp.o ComparisonEngine.o DDL_DML.o QueryPlan.o  -lfl -lpthread
    
main.o : main.cc
	$(CC) -g -c main.cc

a4-1.out: Statistics.o Record.o Comparison.o ComparisonEngine.o Schema.o RelOp.o File.o EventLogger.o FileUtil.o DBFile.o Heap.o Sorted.o Pipe.o BigQ.o y.tab.o lex.yy.o yyfunc.tab.o lex.yyfunc.o test.o
	$(CC) -o a4-1.out y.tab.o lex.yy.o Statistics.o   Record.o Schema.o Function.o RelOp.o Comparison.o File.o EventLogger.o FileUtil.o Heap.o Sorted.o DBFile.o Pipe.o BigQ.o yyfunc.tab.o lex.yyfunc.o  ComparisonEngine.o  test.o -lfl -lpthread

a4.out: y.tab.o lex.yy.o test.o Statistics.o Optimizer.o Record.o Schema.o Function.o Comparison.o File.o EventLogger.o FileUtil.o Heap.o Sorted.o DBFile.o Pipe.o BigQ.o RelOp.o ComparisonEngine.o DDL_DML.o QueryPlan.o
	$(CC) -o a4.out y.tab.o lex.yy.o Statistics.o Optimizer.o test.o Record.o  Schema.o Function.o Comparison.o File.o EventLogger.o FileUtil.o Heap.o Sorted.o DBFile.o Pipe.o BigQ.o RelOp.o ComparisonEngine.o DDL_DML.o QueryPlan.o  -lfl -lpthread

a3.out: Record.o Comparison.o ComparisonEngine.o Schema.o File.o EventLogger.o FileUtil.o Heap.o Sorted.o DBFile.o Pipe.o BigQ.o RelOp.o Function.o y.tab.o yyfunc.tab.o lex.yy.o lex.yyfunc.o a3test.o
	$(CC) -o a3.out Record.o Comparison.o ComparisonEngine.o Schema.o File.o EventLogger.o FileUtil.o Heap.o Sorted.o DBFile.o Pipe.o BigQ.o RelOp.o Function.o y.tab.o yyfunc.tab.o lex.yy.o lex.yyfunc.o a3test.o -lfl -lpthread

a2-2test.out: Record.o Comparison.o ComparisonEngine.o Schema.o File.o FileUtil.o Heap.o Sorted.o BigQ.o DBFile.o Pipe.o y.tab.o lex.yy.o a3test.o EventLogger.o a2-2test.o
	$(CC) -o a2-2test.out Record.o Comparison.o ComparisonEngine.o Schema.o File.o BigQ.o DBFile.o Pipe.o y.tab.o lex.yy.o a2-2test.o EventLogger.o FileUtil.o Heap.o Sorted.o -lfl -lpthread

a2test.out: Record.o Comparison.o ComparisonEngine.o Schema.o File.o FileUtil.o Heap.o Sorted.o BigQ.o DBFile.o Pipe.o y.tab.o lex.yy.o a2-test.o EventLogger.o
	$(CC) -o a2test.out Record.o Comparison.o ComparisonEngine.o Schema.o File.o BigQ.o DBFile.o Pipe.o y.tab.o lex.yy.o a2-test.o EventLogger.o FileUtil.o Heap.o Sorted.o -lfl -lpthread

a1test.out: Record.o Comparison.o ComparisonEngine.o Schema.o File.o FileUtil.o Heap.o Sorted.o BigQ.o DBFile.o Pipe.o EventLogger.o yyold.tab.o lex.yyold.o a1-test.o
	$(CC) -o a1test.out Record.o Comparison.o ComparisonEngine.o Schema.o File.o FileUtil.o Heap.o Sorted.o BigQ.o EventLogger.o DBFile.o Pipe.o yyold.tab.o lex.yyold.o a1-test.o -lfl -lpthread

a3test.o: a3test.cc
	$(CC) -g -c a3test.cc

a2-2test.o: a2-2test.cc
	$(CC) -g -c a2-2test.cc

a2-test.o: a2-test.cc
	$(CC) -g -c a2-test.cc

a1-test.o: a1-test.cc
	$(CC) -g -c a1-test.cc

Statistics.o: Statistics.cc
	$(CC) -g -c Statistics.cc

Optimizer.o: Optimizer.cc
	$(CC) -g -c Optimizer.cc

DDL_DML.o: DDL_DML.cc
	$(CC) -g -c DDL_DML.cc

QueryPlan.o: QueryPlan.cc
	$(CC) -g -c QueryPlan.cc

Heap.o: Heap.cc
	$(CC) -g -c Heap.cc

Sorted.o: Sorted.cc
	$(CC) -g -c Sorted.cc

FileUtil.o: FileUtil.cc
	$(CC) -g -c FileUtil.cc

RelOp.o: RelOp.cc
	$(CC) -g -c RelOp.cc

Function.o: Function.cc
	$(CC) -g -c Function.cc

Comparison.o: Comparison.cc
	$(CC) -g -c Comparison.cc

ComparisonEngine.o: ComparisonEngine.cc
	$(CC) -g -c ComparisonEngine.cc

Pipe.o: Pipe.cc
	$(CC) -g -c Pipe.cc

BigQ.o: BigQ.cc
	$(CC) -g -c BigQ.cc

DBFile.o: DBFile.cc
	$(CC) -g -c DBFile.cc

File.o: File.cc
	$(CC) -g -c File.cc

Record.o: Record.cc
	$(CC) -g -c Record.cc

Schema.o: Schema.cc
	$(CC) -g -c Schema.cc

EventLogger.o: EventLogger.cc
	$(CC) -g -c EventLogger.cc

y.tab.o: Parser.y
	yacc -d Parser.y
	sed $(tag) y.tab.c -e "s/  __attribute__ ((__unused__))$$/# ifndef __cplusplus\n  __attribute__ ((__unused__));\n# endif/" 
	g++ -c y.tab.c
                
yyfunc.tab.o: ParserFunc.y
	yacc -p "yyfunc" -b "yyfunc" -d ParserFunc.y
	#sed $(tag) yyfunc.tab.c -e "s/  __attribute__ ((__unused__))$$/# ifndef __cplusplus\n  __attribute__ ((__unused__));\n# endif/" 
	g++ -c yyfunc.tab.c
        
lex.yy.o: Lexer.l
	lex Lexer.l
	gcc  -c lex.yy.c

lex.yyfunc.o: LexerFunc.l
	lex -Pyyfunc LexerFunc.l
	gcc  -c lex.yyfunc.c


yyold.tab.o: ParserOld.y
	yacc -p "yyold" -b "yyold" -d ParserOld.y
	sed $(tag) yyold.tab.c -e "s/  __attribute__ ((__unused__))$$/# ifndef __cplusplus\n  __attribute__ ((__unused__));\n# endif/" 
	g++ -c yyold.tab.c
        

lex.yyold.o: LexerOld.l
	lex -Pyyold LexerOld.l
	gcc  -c lex.yyold.c
clean: 
	rm -f *.o
	rm -f *.out
	rm -f y.tab.*
	rm -f yyfunc.tab.*
	rm -f lex.yy.*
	rm -f lex.yyfunc*
