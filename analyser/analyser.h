#pragma once

#include "error/error.h"
#include "instruction/instruction.h"
#include "tokenizer/token.h"

#include <vector>
#include <optional>
#include <utility>
#include <map>
#include <cstdint>
#include <cstddef> // for std::size_t

namespace miniplc0 {

	class Analyser final {
	private:
		using uint64_t = std::uint64_t;
		using int64_t = std::int64_t;
		using uint32_t = std::uint32_t;
		using int32_t = std::int32_t;
		struct dataType{
			int no;
			std::pair<int32_t ,int32_t > Index;//<层次，在运行栈上的偏移>，也就是这个常量的位置
			TokenType type;//string\int\char\double
			bool flag= false;//是否初始化
			bool isConst = false;
			dataType(){}
			dataType(std::pair<int32_t ,int32_t > n,TokenType t,bool f,bool is){
				Index = n;
				type = t;
				flag = f;
				isConst = is;
			}
		};
		struct paramList{
			int params_size = 0;//参数占用的slot数
			int params_num = 0;//参数的个数
			std::vector<TokenType> paras_list = std::vector<TokenType>();//参数的类型，按照顺序
			paramList(){}
		};
		struct functionType{
			int index;
			TokenType retType;
			paramList list = paramList();
			functionType(){}
			functionType(int i,TokenType t){
				index = i;
				retType = t;
			}
		};

	public:
		Analyser(std::vector<Token> v,std::ostream& output,int __type)
			: _tokens(std::move(v)), _offset(0), _instructions_s({}), _current_pos(0, 0),
			 _nextTokenIndex(0),_output(output),output_type(__type) {}
		Analyser(Analyser&&) = delete;
		Analyser(const Analyser&) = delete;
		Analyser& operator=(Analyser) = delete;

		// 唯一接口
		std::pair<std::vector<std::vector<std::string>>, std::optional<CompilationError>> Analyse();
	private:
		// 所有的递归子程序
		std::optional<CompilationError> analyseProgram();

		std::optional<CompilationError> analyseVariableDeclaration();

		std::optional<CompilationError> analyseStatementSequence();

		std::optional<CompilationError> analyseExpression();

        std::optional<CompilationError> analyseInitDeclarator(TokenType type,bool isConst);

        std::optional<CompilationError> analyseFunctionDefinition();

        std::optional<CompilationError> analyseCompoundStat();

        std::optional<CompilationError> analyseParametterList();

        std::optional<CompilationError> analyseAdditiveExp();

        std::optional<CompilationError> analyseMultiveExp();

        std::optional<CompilationError> analyseUnaryExp();

        std::optional<CompilationError> analysePrimaryExp();

        std::optional<CompilationError> analyseFunctionCall();

        std::optional<CompilationError> analyseExpressionList(int no);

        std::optional<CompilationError> analyseAssignmentExpression();

		std::optional<CompilationError> analyseCondition(int32_t& no);

		std::optional<CompilationError> analyseConditionStatement();

		std::optional<CompilationError> analyseLoopStatement();

		std::optional<CompilationError> analyseJumpStatement();

		std::optional<CompilationError> analysePrintStatement();

		std::optional<CompilationError> analysePrintableList();

		std::optional<CompilationError> analyseScanStatement();

		std::optional<CompilationError> analyseStatement();

		//下面是字符判断操作
		bool istypeSpecifier(TokenType t);

		bool isAdditiveOp(TokenType t);

		bool isMultiplicativeOp(TokenType t);

		bool isRelationalOp(TokenType t);

		bool isFunction(std::string s);

		void addParamList(paramList& list,TokenType t);

		int _instructions_jump(TokenType relationOP);

		void _setJmpAddr(int no){
			_instructions_s[_functions_nums][no]+=" "+std::to_string(_instructions_s[_functions_nums].size());
		}

		// Token 缓冲区相关操作

		// 返回下一个 token
		std::optional<Token> nextToken();
		// 回退一个 token
		void unreadToken();

		// 下面是符号表相关操作
		//add
		void _add(const Token& name, TokenType value_type,bool flag, bool isConst);
		void _add_function(std::string name,TokenType type);

		//add
		// 是否被声明过

		//函数和全局变量定义的时候查重名：
		//1. 查所有定义过的函数有没有重名
		//2. 查全局变量:vector[0]->map
		bool isDeclared(const std::string&);

		//函数中的变量查重名：
		//1. 查当前函数的定义域内有没有重名
		//2. 看是否和所在的函数重名
		bool varIsDeclared(const std::string&);

		void testPrint(std::string s){
			std::cout<<s<<std::endl;
		}
		int Getnos(){
			int nos;
			if(_instructions_s[_functions_nums].size()==0){
				nos = 0;
			} else{
				nos = _instructions_s[_functions_nums].size();
			}
			return nos;
		}
	private:
		std::vector<Token> _tokens;
		std::size_t _offset;
	//	std::vector<Instruction> _instructions;
		std::pair<uint64_t, uint64_t> _current_pos;

		//常量表：int、char、double,Identifier(function name)
		std::vector <std::map<std::string, dataType>> _constants = std::vector <std::map<std::string, dataType>>();

		//function table
		std::map<std::string, functionType> _functions = std::map<std::string, functionType>();
		std::vector<std::string>_functionsTable = std::vector<std::string>();//用来存储标号-》name

		int32_t _functions_nums = 0;


		// 下一个 token 在栈的偏移
		int32_t _nextTokenIndex = 0;

		//int jump_no;//用来记录跳转指令的标号
		std::vector<std::vector<std::string>> _instructions_s;

		//output
		void outputInstructions();
		void outputBitsFile();
		std::ostream& _output;
		bool output_type = false;
		std::string changeNum(int n );
       void outputOP(std::string );
       int getX(int []);
	};
}