#include "analyser.h"

#include <climits>
#include <cstring>
#include <3rd_party/fmt/include/fmt/color.h>

namespace miniplc0 {
	std::pair<std::vector<std::vector<std::string>>, std::optional<CompilationError>> Analyser::Analyse() {
		auto err = analyseProgram();
		if (err.has_value())
			return std::make_pair(std::vector<std::vector<std::string>>(), err);
		else
			return std::make_pair(_instructions_s, std::optional<CompilationError>());
	}

	// <C0-program> ::=
	//	  {<variable-declaration>}{<function-definition>}
	std::optional<CompilationError> Analyser::analyseProgram() {
		//必须先把全局的constant添加进入vector，否则就会出错
		std::map<std::string, dataType> map0 = std::map<std::string, dataType>();
		_constants.push_back(map0);
		_functionsTable.push_back("000");
		std::vector<std::string> s;
		_instructions_s.emplace_back(s);
		//std::cout<<_instructions_s.size()<<std::endl;
		// {<variable-declaration>}
		auto err = analyseVariableDeclaration();
		if(err.has_value()){
            return err;
		}
		// {<function-definition>}
		err = analyseFunctionDefinition();
		if (err.has_value())
			return err;

		//c0要求一定要有main函数，所以分析完成后如果没有main就报错
		if(_functions.find("main") == _functions.end()){
			return std::make_optional<CompilationError>(_current_pos,ErrorCode::ErrNoMain);
		}


		    outputInstructions();
//std::cout<<"why"<<std::endl;
		return {};
	}

	//todo：对于所有的变量声明函数，尚未解决的问题有：
	//todo：1. 标志服作用域的问题
    //<variable-declaration> ::=
    //    [<const-qualifier>]<type-specifier><init-declarator-list>';'
	//语义规则：
	// 变量的类型，不能是void或const void
	// const修饰的变量必须被显式初始化
	// const修饰的变量不能被修改
	// 声明为变量的标识符，在同级作用域中只能被声明一次
	//UB： 未初始化的非const变量，C0不要求对其提供默认值，因此使用它们是未定义行为。
	// 你可以为他们指定一个默认值，也可以选择报出编译错误，也可以给出一个warning之后当作无事发生。
    std::optional<CompilationError> Analyser::analyseVariableDeclaration() {
	    //testPrint("analyseVariableDeclaration");
        // 变量声明语句可能有0个或者多个
        while (true){
            // 预读:变量声明部分开头的token可能是const类型或者是type类型，如果不是就回退
			int count = 0;
            auto next = nextToken();
            count++;
            if(!next.has_value()){
            	unreadToken();
                return {};
            }
            // <const-qualifier> or <type-specifier>
            if(next.value().GetType() != TokenType::CONST && !istypeSpecifier(next.value().GetType())){
                unreadToken();
                return {};//说明不是变量声明语句
            }
            // <const-qualifier>:此时存储的变量是常量类型，不能改动他的值。
            bool isConst = false;
            if(next.value().GetType() == TokenType::CONST){
                isConst = true;
                next = nextToken();
                count++;
                if(!next.has_value()){
                    return std::make_optional<CompilationError>(_current_pos,ErrorCode::ErrInvalidVariableDeclaration);
                }
            }
            // <type-specifier>
			//bug:不能在这里就判断void，因为可能是函数，函数可以是void
            if(!istypeSpecifier(next.value().GetType())){
            	unreadToken();
				return std::make_optional<CompilationError>(_current_pos,ErrorCode::ErrInvalidVariableDeclaration);
			}
			TokenType type = next.value().GetType();
			//预读：用来区分fucdef和vardef
			//std::cout<<"fir"<<std::endl;
			next = nextToken();//identifier
				//	std::cout<<"sec"<<std::endl;
			next = nextToken();//(
            count++;
            count++;
            if(!next.has_value()){
            	unreadToken();
            	unreadToken();
            } else if(next.value().GetType() == LEFT_BRACKET){
            	//is function
            	while (count--){
            		unreadToken();
            	}
				return {};
            } else{
            	//is variable
            	unreadToken();
            	unreadToken();
            }
            //此时再判断类型是否是void
			if(type == VOID){
				return std::make_optional<CompilationError>(_current_pos,ErrorCode::ErrInvalidVariableDeclaration);
            }
			//<init-declarattor-list>
			auto err = analyseInitDeclarator(type,isConst);
            if(err.has_value()){
				return err;
            }
            // ';'
            next = nextToken();
            if(!next.has_value()||next.value().GetType()!=TokenType::SEMICOLON){
                return std::make_optional<CompilationError>(_current_pos, ErrorCode::ErrNoSemicolon);
            }
        }
        return {};
    }

    //<init-declarator-list> ::=
    //    <init-declarator>{','<init-declarator>}
    //<init-declarator> ::=
    //    <identifier>[<initializer>]
    //<initializer> ::=
    //    '='<expression>
    std::optional<CompilationError> Analyser::analyseInitDeclarator(TokenType type,bool isConst) {
	    //testPrint("analyseInitDeclarator");
	    //<init-declarator>
		//<init-declarator> ::=
		//    <identifier>[<initializer>]
		//std::cout<<type<<" "<<(type == INT )<<std::endl;
		auto next = nextToken();
		if(!next.has_value()){
			return std::make_optional<CompilationError>(_current_pos,ErrorCode::ErrEOF);
		}
		if(next.value().GetType() != TokenType::IDENTIFIER){
			unreadToken();
			return std::make_optional<CompilationError>(_current_pos,ErrorCode::ErrNeedIdentifier);
		}
		//预读一个
		auto name = next;
		next = nextToken();
		if(!next.has_value() || next.value().GetType() != TokenType::EQUAL_SIGN){
			unreadToken();
			//flag==true means that the type of var is const,and it should be initialated
			if(isConst){
				return std::make_optional<CompilationError>(_current_pos,ErrorCode::ErrConstantNeedValue);
			}

			if(varIsDeclared(name.value().GetValueString())){
				return std::make_optional<CompilationError>(_current_pos,ErrorCode::ErrDuplicateDeclaration) ;
			}
			//添加一个没有初始化的变量
			//因为懒得判断，所以直接push一个0
			_add(name.value(),type, false,isConst);
			_instructions_s[_functions_nums].emplace_back(std::string(std::to_string(Getnos())+"    ipush 0"));
		}
		else{
			auto err = analyseExpression();
			if(err.has_value()){
				return err;
			}
			//todo:添加当前的标志符号
			if(varIsDeclared(name.value().GetValueString())){
				return std::make_optional<CompilationError>(_current_pos,ErrorCode::ErrDuplicateDeclaration) ;
			}
			//加入变量表
			_add(name.value(),type, true,isConst);
			//指令
			//todo:可能不需要指令，因为变量声明早于所有的函数声明，所以表达式会直接将值推入栈中，刚好就是应该在的位置
		}

		//{','<init-declarator>}
		while (true){
			//','
			next = nextToken();
			if(!next.has_value() || next.value().GetType() != TokenType::COMMA){
				unreadToken();
				return {};
			}
			//<init-declarator>
			next = nextToken();
			if(!next.has_value()){
				return std::make_optional<CompilationError>(_current_pos,ErrorCode::ErrEOF);
			}
			if(next.value().GetType() != TokenType::IDENTIFIER){
				unreadToken();
				return std::make_optional<CompilationError>(_current_pos,ErrorCode::ErrNeedIdentifier);
			}
			//预读一个
			name = next;
			next = nextToken();
			if(!next.has_value() || next.value().GetType() != TokenType::EQUAL_SIGN){
				unreadToken();
				//flag==true means that the type of var is const,and it should be initialated
				if(isConst){
					return std::make_optional<CompilationError>(_current_pos,ErrorCode::ErrConstantNeedValue);
				}

				if(varIsDeclared(name.value().GetValueString())){
					return std::make_optional<CompilationError>(_current_pos,ErrorCode::ErrDuplicateDeclaration) ;
				}
				//添加一个没有初始化的变量
				_add(name.value(),type, false,isConst);
				_instructions_s[_functions_nums].emplace_back(std::string(std::to_string(Getnos())+"    ipush 0"));
			}
			else{
				auto err = analyseExpression();
				if(err.has_value()){
					return err;
				}
				//todo:添加当前的标志符号
				if(varIsDeclared(name.value().GetValueString())){
					return std::make_optional<CompilationError>(_current_pos,ErrorCode::ErrDuplicateDeclaration) ;
				}
				//加入变量表
				_add(name.value(),type, true,isConst);
				//指令
				//todo:可能不需要指令，因为变量声明早于所有的函数声明，所以表达式会直接将值推入栈中，刚好就是应该在的位置
			}
		}
		return {};

	}

	//<function-definition> ::=
	//    <type-specifier><identifier><parameter-clause><compound-statement>
	//<parameter-clause> ::=
	//    '(' [<parameter-declaration-list>] ')']
	//<function-call> ::=
	//    <identifier> '(' [<expression-list>] ')'
	//<expression-list> ::=
	//    <expression>{','<expression>}
	std::optional<CompilationError> Analyser::analyseFunctionDefinition(){
	    //testPrint("analyseFunctionDefinition");
		while (true){
			//function的开头，进行计数器的初始设置
			_functions_nums++;//也就是当前函数的序号
			_nextTokenIndex = 0;
			//<type-specifier>
			auto next = nextToken();
			if(!next.has_value()){
				return {};
			}
			if(!istypeSpecifier(next.value().GetType())){
				unreadToken();
				return {};
			}
			TokenType type = next.value().GetType();
			//<identifier>
			next = nextToken();
			auto name  = next;
			if(!next.has_value() || next.value().GetType() != IDENTIFIER){
				unreadToken();
				return std::make_optional<CompilationError>(_current_pos,ErrorCode::ErrNeedIdentifier);
			}
			//加入函数表
			//std::cout<<"function1"<<std::endl;
			if(isDeclared(name.value().GetValueString())){
				return std::make_optional<CompilationError>(_current_pos,ErrorCode::ErrDuplicateDeclaration);
			}
			_add_function(name.value().GetValueString(),type);

			//<parameter-clause>
			next = nextToken();
			//'('
			if(!next.has_value() || next.value().GetType() != TokenType::LEFT_BRACKET){
				return std::make_optional<CompilationError>(_current_pos,ErrorCode::ErrInvalidParameterList);
			}
			//[<parameter-declaration-list>]
			auto err = analyseParametterList();
			if(err.has_value()){
				return err;
			}
			//')'
			next = nextToken();
			if(!next.has_value() || next.value().GetType() != TokenType::RIGHT_BRACKET){
				return std::make_optional<CompilationError>(_current_pos,ErrorCode::ErrInvalidParameterList);
			}

			//<compound-statement>
			err = analyseCompoundStat();
			if(err.has_value()){
				return err;
			}
			//对于每一个函数，不管三七二十一加一个返回语句
			std::string retState = std::string();
			if(_functions[_functionsTable[_functions_nums]].retType == VOID){
				_instructions_s[_functions_nums].emplace_back(std::string(std::to_string(Getnos())+"    "+"ret"));
				//std::string retState = std::string(std::to_string(Getnos())+"    "+"ret");
			} else{
				_instructions_s[_functions_nums].emplace_back(std::string(std::to_string(Getnos())+"    "+"ipush 1"));
				_instructions_s[_functions_nums].emplace_back(std::string(std::to_string(Getnos())+"    "+"iret"));
				//std::string retState = std::string(std::to_string(Getnos())+"    "+"ipush 1\n"+std::to_string(Getnos()+1)+"    iret");
			}
		}
		return {};

	}


	//<parameter-declaration-list> ::=
	//    <parameter-declaration>{','<parameter-declaration>}
	//<parameter-declaration> ::=
	//    [<const-qualifier>]<type-specifier><identifier>
	std::optional<CompilationError> Analyser::analyseParametterList() {
	    //testPrint("analyseParametterList");
		//由于parameterlist可以是0个或者1个，所以即使第一个parameterdef没有也是正确的
		//<parameter-declaration>
		auto next = nextToken();
		if(!next.has_value() || (next.value().GetType() != TokenType::CONST && !istypeSpecifier(next.value().GetType()))){
			unreadToken();
			////testPrint("PPPPPPPPP");
			return {};
		}
		//    [<const-qualifier>]
		bool isConst = false;
		if(next.value().GetType() == TokenType::CONST){
			isConst = true;
			next = nextToken();
			if(!next.has_value()){
				return std::make_optional<CompilationError>(_current_pos,ErrorCode::ErrInvalidFunctionDeclaration);
			}
		}
		//<type-specifier>
		if(!istypeSpecifier(next.value().GetType())||next.value().GetType() == VOID){
			unreadToken();
			return std::make_optional<CompilationError>(_current_pos,ErrorCode::ErrInvalidFunctionDeclaration);
		}
		TokenType type = next.value().GetType();
		//<identifier>
		next = nextToken();
		if(!next.has_value() || next.value().GetType()!=TokenType::IDENTIFIER){
			return std::make_optional<CompilationError>(_current_pos,ErrorCode::ErrNeedIdentifier);
		}
		//todo:存储这个函数param
		addParamList(_functions[_functionsTable[_functions_nums]].list,type);
		_add(next.value(),type, false, isConst);
		//{','<parameter-declaration>}
		while (true){
			//预读 ','
			next = nextToken();
			if(!next.has_value() || next.value().GetType() != TokenType::COMMA){
				unreadToken();
				return {};
			}

			next = nextToken();
			//  [<const-qualifier>]
			isConst = false;
			if(next.value().GetType() == TokenType::CONST){
				isConst = true;
				next = nextToken();
				if(!next.has_value()){
					return std::make_optional<CompilationError>(_current_pos,ErrorCode::ErrInvalidFunctionDeclaration);
				}
			}
			//<type-specifier>
			if(!istypeSpecifier(next.value().GetType()) || next.value(). GetType()==VOID){
				return std::make_optional<CompilationError>(_current_pos,ErrorCode::ErrInvalidFunctionDeclaration);
			}
			type = next.value().GetType();
			//<identifier>
			next = nextToken();
			if(!next.has_value() || next.value().GetType()!=TokenType::IDENTIFIER){
				return std::make_optional<CompilationError>(_current_pos,ErrorCode::ErrNeedIdentifier);
			}
			//todo:存储这个函数param
			addParamList(_functions[_functionsTable[_functions_nums]].list,type);
			_add(next.value(),type, false, isConst);
		}
		return {};
	}

	//<compound-statement> ::=
	//    '{' {<variable-declaration>} <statement-seq> '}'
	//语义规则：
	//函数名不能被修改，也不可读（因此不参与计算）
	//参数的类型，不能是void或const void
	//const修饰的参数不能被修改
	//声明为函数的标识符，在同级作用域中只能被声明一次
	//声明为参数的标识符，在同级作用域中只能被声明一次
	//函数调用的标识符，必须是当前作用域中可见的，被声明为函数的标识符
	//函数传参均为拷贝传值，在被调用者中对参数值的修改，不应该导致调用者也被修改
	//函数调用的传参数量以及每一个参数的数据类型（不考虑const），都必须和函数声明中的完全一致
	//不能在返回类型为void的函数中使用有值的返回语句，也不能在非void函数中使用无值的返回语句。
	//UB: 函数中的控制流，如果存在没有返回语句的分支，这些分支的具体返回值是未定义行为。但是你的汇编必须保证每一种控制流分支都能够返回。
	std::optional<CompilationError> Analyser::analyseCompoundStat(){
	    //testPrint("analyseCompoundStat");
		//'{'
		//std::cout<<"?"<<std::endl;
		auto next = nextToken();
		if(!next.has_value() || next.value().GetType() != TokenType::LEFT_BRACE){
			return std::make_optional<CompilationError>(_current_pos,ErrorCode::ErrInvalidCompundStatement);
		}
		//{variable-declaration}
		auto err = analyseVariableDeclaration();
		if(err.has_value()){
			return err;
		}
		//statement-seq
		err = analyseStatementSequence();
		if(err.has_value()){
			return err;
		}
		//'}'
		next = nextToken();
		if(!next.has_value() || next.value().GetType() != TokenType::RIGHT_BRACE){
			return std::make_optional<CompilationError>(_current_pos,ErrorCode::ErrInvalidCompundStatement);
		}
		return {};
	}

	//<statement-seq> ::=
	//	{<statement>}
	//<statement> ::=
	//     '{' <statement-seq> '}'
	//    |<condition-statement>
	//    |<loop-statement>
	//    |<jump-statement>
	//    |<print-statement>
	//    |<scan-statement>
	//    |<assignment-expression>';'
	//    |<function-call>';'
	//    |';'
	std::optional<CompilationError> Analyser::analyseStatementSequence() {
	    //testPrint("analyseStatementSequence");
		auto next = nextToken();
		if(!next.has_value()){
			return {};
		}
		TokenType t = next.value().GetType();
		while (t == LEFT_BRACE || t == IF ||t == WHILE ||t == FOR || t == DO||t == RETURN||t == BREAK || t == CONTINUE
				|| t == PRINT || t == SCAN || t == SEMICOLON || t == IDENTIFIER){
			unreadToken();
			auto err = analyseStatement();
			if(err.has_value()){
				return err;
			}
			next = nextToken();
			if(!next.has_value()){
				return {};
			}
			t = next.value().GetType();
		}
		unreadToken();
		return {};
	}
	std::optional<CompilationError> Analyser::analyseStatement() {
	    //testPrint("analyseStatement");
		auto next = nextToken();
		if(!next.has_value()){
			return std::make_optional<CompilationError>(_current_pos,ErrorCode::ErrEOF);
		}
		TokenType type = next.value().GetType();
		if(type == LEFT_BRACE){
			//'{'
			// <statement-seq>
			auto err = analyseStatementSequence();
			if(err.has_value()){
				return err;
			}
			//'}'
			next = nextToken();
			if(!next.has_value() || next.value().GetType() != TokenType::RIGHT_BRACE){
				return std::make_optional<CompilationError>(_current_pos,ErrorCode::ErrNeedRightBrace);
			}
		} else if(type == IF ){
			unreadToken();
			auto err = analyseConditionStatement();
			if(err.has_value()){
				return err;
			}
		} else if(type==WHILE || type==DO || type == FOR){
			unreadToken();
			auto err = analyseLoopStatement();
			if(err.has_value()){
				return err;
			}
		} else if(type == RETURN || type == BREAK ||type == CONTINUE){
			unreadToken();
			auto err = analyseJumpStatement();
			if(err.has_value()){
				return err;
			}
		} else if(type == PRINT){
			unreadToken();
			auto err = analysePrintStatement();
			if(err.has_value()){
				return err;
			}
		} else if(type == SCAN){
			unreadToken();
			auto err = analyseScanStatement();
			if(err.has_value()){
				return err;
			}
		} else if(type == IDENTIFIER){
			unreadToken();
			if(isFunction(next.value().GetValueString())){
				auto err = analyseFunctionCall();
				if(err.has_value()){
					return err;
				}
			} else{
				auto err = analyseAssignmentExpression();
				if(err.has_value()){
					return  err;
				}
			}
		} else if(type == SEMICOLON){

		} else{
			unreadToken();
			return std::make_optional<CompilationError>(_current_pos,ErrorCode::ErrInvalidStatement);
		}
		return {};
	}

	//<condition-statement> ::=
	//    'if' '(' <condition> ')' <statement> ['else' <statement>]
	//语义：
	//else总是匹配前文中距离其最近的尚且没有和else匹配的if
	//if语句的流程是：
	//求值<condition>
	//如果<condition>是true(!=0)，控制进入if的代码块
	//如果<condition>是false==0，控制进入else的代码块

	//<condition> ::=
	//    <expression>[<relational-operator><expression>]
	//语义规则：<condition>只有true和false的逻辑语义，其运算数都必须是有值的（不能是void类型、不能是函数名）
	//<condition> ::= <expression>时，如果<expression>是（或可以转换为）int类型，且转换得到的值为0，那么视为false；否则均视为true。
	//<condition> ::= <expression><relational-operator><expression>时，根据对应关系运算符的语义，决定是true还是false
	std::optional<CompilationError> Analyser::analyseCondition(int32_t& jump_no){
	    //testPrint("analyseCondition");
		//condition:<expression>[<relational-operator><expression>]
		//<expression>
		auto err = analyseExpression();
		if(err.has_value()){
			return err;
		}
		//预读
		//relattional-op
		auto next = nextToken();
		if(!next.has_value() || !isRelationalOp(next.value().GetType())){
			unreadToken();
			//此时condition的形式是if（expression）
			//指令
			_instructions_s[_functions_nums].emplace_back(std::string(std::to_string(Getnos())+"    "+"je"));
			//_instructions.emplace_back(Operation::je,0,Getnos());//此处的0只是用来占位的。
			jump_no = _instructions_s[_functions_nums].size()-1;
		} else{
			TokenType relationOP = next.value().GetType();
			//<expression>
			err = analyseExpression();
			if(err.has_value()){
				return err;
			}
			//instruction
			jump_no = _instructions_jump(relationOP);
		}
		return {};
	}
	std::optional<CompilationError> Analyser::analyseConditionStatement() {
	    //testPrint("analyseConditionStatement");
		auto next = nextToken();
		//'if'
		if(!next.has_value() || next.value().GetType() != IF){
			return std::make_optional<CompilationError>(_current_pos,ErrorCode::ErrInvalidConditionStatement);
		}
		//'('
		next = nextToken();
		if(!next.has_value() || next.value().GetType() != LEFT_BRACKET){
			return std::make_optional<CompilationError>(_current_pos,ErrorCode::ErrInvalidConditionStatement);
		}
		int32_t jump_no;
		auto err = analyseCondition(jump_no);
		if(err.has_value()){
			return err;
		}
		//')'
		next = nextToken();
		if(!next.has_value() || next.value().GetType() != RIGHT_BRACKET){
			return std::make_optional<CompilationError>(_current_pos,ErrorCode::ErrInvalidConditionStatement);
		}

		//<statement>
		err = analyseStatement();
		if(err.has_value()){
			return err;
		}
		//实际上，当分析完紧跟着if的函数体后，就应该设置跳转地址，因为跳转的地址永远是第一个函数体后面的第一条语句
		//此处有一个bug，假如有else的话，会在第一个模块的最后加一个跳转指令，所以如果有else就应该跳转到下下条指令

		//['else' <statement>]
		//预读一个
		next = nextToken();
		if(!next.has_value() || next.value().GetType() != ELSE){
			_setJmpAddr(jump_no);
			unreadToken();
			return {};
		}else{
			//由于有了else，所以第一个if后面紧跟着的函数体最后要加上一段跳转指令，跳转到else函数体的后一条语句
			_instructions_s[_functions_nums].emplace_back(std::string(std::to_string(Getnos())+"    "+"jmp"));
		//	_instructions.emplace_back(Operation::jmp,0,Getnos());
			//std::cout<<"jumpno"<<jump_no<<std::endl;
			_setJmpAddr(jump_no);//应该在添加上了jmp指令之后在设置原先的jne指令的跳转位置
			jump_no = _instructions_s[_functions_nums].size()-1;
			err = analyseStatement();
			if(err.has_value()){
				return err;
			}
		//	std::cout<<"jumpno"<<jump_no<<std::endl;
			_setJmpAddr(jump_no);
		}
		return {};
	}

	//<loop-statement> ::=
	//    'while' '(' <condition> ')' <statement>
	//   |'do' <statement> 'while' '(' <condition> ')' ';'
	//   |'for' '('<for-init-statement> [<condition>]';' [<for-update-expression>]')' <statement>
	std::optional<CompilationError> Analyser::analyseLoopStatement() {
	    //testPrint("analyseLoopStatement");
		auto next = nextToken();
		int startno;//记录循环的入口
		switch(next.value().GetType()){
			case WHILE:{
				next = nextToken();
				//'('
				if(!next.has_value() ||next.value().GetType() != LEFT_BRACKET){
					unreadToken();
					return std::make_optional<CompilationError>(_current_pos,ErrorCode::ErrInvalidLoopStatement);
				}
				//condition
				startno = _instructions_s[_functions_nums].size();
				int32_t jump_no;
				auto err = analyseCondition(jump_no);
				if(err.has_value()){
					return err;
				}
				//')'
				next = nextToken();
				if(!next.has_value() ||next.value().GetType() != RIGHT_BRACKET){
					unreadToken();
					return std::make_optional<CompilationError>(_current_pos,ErrorCode::ErrInvalidLoopStatement);
				}
				//<statement>
				err = analyseStatement();
				if(err.has_value()){
					return err;
				}
				//执行完循环体后应该跳转到循环入口
				_instructions_s[_functions_nums].emplace_back(std::string(std::to_string(Getnos())+"    "+"jmp"+" "+std::to_string(startno)));
				//修改地址
				_instructions_s[_functions_nums][jump_no] += " " +std::to_string(_instructions_s[_functions_nums].size());
				break;
			}

		}
		return {};
	}

	//<expression> ::=
	//    <additive-expression>
	std::optional<CompilationError> Analyser::analyseExpression() {
	    //testPrint("analyseExpression");
		auto err = analyseAdditiveExp();
		if(err.has_value()){
			return err;
		}
		return {};
	}

	//<additive-expression> ::=
	//    <multiplicative-expression>{<additive-operator><multiplicative-expression>}
	std::optional<CompilationError> Analyser::analyseAdditiveExp() {
	    //testPrint("analyseAdditiveExp");
		//multiplicativeexp
		auto err = analyseMultiveExp();
		if(err.has_value()){
			return err;
		}
		while (true){
			//additive op
			auto next = nextToken();
			if(!next.has_value()||!isAdditiveOp(next.value().GetType())){
				unreadToken();
				return {};
			}

			//multiplicative exp
			err = analyseMultiveExp();
			if(err.has_value()){
				return err;
			}

			//todo:根据op来操作
			switch (next.value().GetType()){
				case TokenType :: PLUS_SIGN :{
					//_instructions.emplace_back(Operation::iadd,Getnos());
					_instructions_s[_functions_nums].emplace_back(std::string(std::to_string(Getnos())+"    "+"iadd"));
					break;
				}

				case TokenType :: MINUS_SIGN:{
					//_instructions.emplace_back(Operation::isub,Getnos());
					_instructions_s[_functions_nums].emplace_back(std::string(std::to_string(Getnos())+"    "+"isub"));
					break;
				}
			}
		}
		return {};
	}

	//<multiplicative-expression> ::=
	//    <unary-expression>{<multiplicative-operator><unary-expression>}
	std::optional<CompilationError> Analyser::analyseMultiveExp() {
	    //testPrint("analyseMultiveExp");
		auto err = analyseUnaryExp();
		if(err.has_value()){
			return err;
		}

		while (true){
			//multive op
			auto next = nextToken();
			if(!next.has_value()||!isMultiplicativeOp(next.value().GetType())){
				unreadToken();
				return {};
			}

			//unary exp
			err = analyseUnaryExp();
			if(err.has_value()){
				return err;
			}

			//todo:根据op来操作
			switch (next.value().GetType()){
				case TokenType :: MULTIPLICATION_SIGN :{
					//_instructions.emplace_back(Operation::imul,Getnos());
					_instructions_s[_functions_nums].emplace_back(std::string(std::to_string(Getnos())+"    "+"imul"));
					break;
				}

				case TokenType :: DIVISION_SIGN:{
					//_instructions.emplace_back(Operation::idiv,Getnos());
					_instructions_s[_functions_nums].emplace_back(std::string(std::to_string(Getnos())+"    "+"idiv"));
					break;
				}
			}
		}
		return {};
	}

	//<unary-expression> ::=
	//    [<unary-operator>]<primary-expression>
	std::optional<CompilationError> Analyser::analyseUnaryExp() {
	    //testPrint("analyseUnaryExp");
		//[<unary-operator>]
		auto next = nextToken();
		if(!next.has_value()){
			unreadToken();
			return std::make_optional<CompilationError>(_current_pos,ErrorCode::ErrNeedUnaryExpression);
		}

		bool flag = true;//代表默认为正
		if(isAdditiveOp(next.value().GetType())){
			if(next.value().GetType() == TokenType::MINUS_SIGN){
				flag = false;
			}
		} else{
			unreadToken();
		}

		//<primary-expression>
		auto err = analysePrimaryExp();
		if(err.has_value()){
			return err;
		}
        if(!flag)
		    //_instructions.emplace_back(Operation::ineg,Getnos());
			_instructions_s[_functions_nums].emplace_back(std::string(std::to_string(Getnos())+"    "+"ineg"));
		return {};
	}

	//<primary-expression> ::=
	//     '('<expression>')'
	//    |<identifier>
	//    |<integer-literal>
	//    |<function-call>
	//注意：参与表达式运算的不可以是void
	std::optional<CompilationError> Analyser::analysePrimaryExp() {
	    //testPrint("analysePrimaryExp");
		//std::cout<<"primariExp"<<std::endl;
		auto next = nextToken();
		if(!next.has_value()){
			unreadToken();
			return std::make_optional<CompilationError>(_current_pos,ErrorCode::ErrInvalidPrimaryExpression);
		}
		//'(' or identifier or integer
		if(next.value().GetType() == TokenType::LEFT_BRACKET){
			////testPrint("1");
			auto err = analyseExpression();
			if(err.has_value()){
				return err;
			}
			//')'
			next = nextToken();
			if(!next.has_value()||next.value().GetType() != TokenType::RIGHT_BRACKET){
				unreadToken();
				return std::make_optional<CompilationError>(_current_pos,ErrorCode::ErrIncompleteExpression);
			}
		} else if (next.value().GetType() == TokenType::IDENTIFIER){
			//function-call
			if(isFunction(next.value().GetValueString())){
				if(_functions[next.value().GetValueString()].retType == VOID){
					return std::make_optional<CompilationError>(_current_pos,ErrorCode::ErrInvalidVoidInExpression);
				}
				unreadToken();
				auto err = analyseFunctionCall();
				if(err.has_value()){
					return err;
				}
				//identifier
			} else{
				//判断当前的标志服好是否可以找到
				std::pair<int32_t ,int32_t > pos;
				if(_constants[_functions_nums].find(next.value().GetValueString()) != _constants[_functions_nums].end()){
					//内部的变量
					if(_constants[_functions_nums][next.value().GetValueString()].type == VOID){
						return std::make_optional<CompilationError>(_current_pos,ErrorCode::ErrInvalidVoidInExpression);
					}
					pos = _constants[_functions_nums][next.value().GetValueString()].Index;
				} else if(_constants[0].find(next.value().GetValueString()) != _constants[0].end()){
					if(_constants[0][next.value().GetValueString()].type == VOID){
						return std::make_optional<CompilationError>(_current_pos,ErrorCode::ErrInvalidVoidInExpression);
					}
					pos = _constants[0][next.value().GetValueString()].Index;
				} else{
					return std::make_optional<CompilationError>(_current_pos,ErrorCode::ErrNotDeclared);
				}
				//testPrint("loada");
				//testPrint(next.value().GetValueString());
			//	std::cout<<pos.first<<" "<<pos.second<<std::endl;
				if(_functions_nums == 0){
					_instructions_s[_functions_nums].emplace_back(std::string(std::to_string(Getnos())+"    "+"loada"+" " +std::to_string(pos.first-1)+", "+std::to_string(pos.second)));
				} else{
					_instructions_s[_functions_nums].emplace_back(std::string(std::to_string(Getnos())+"    "+"loada"+" " +std::to_string(pos.first)+", "+std::to_string(pos.second)));
				}
				//_instructions_s[_functions_nums].emplace_back(std::string(std::to_string(Getnos())+"    "+"loada"+" " +std::to_string(pos.first)+", "+std::to_string(pos.second)));
				_instructions_s[_functions_nums].emplace_back(std::string(std::to_string(Getnos())+"    "+"iload"));
			}
		} else if(next.value().GetType() == TokenType:: UNSIGNED_INTEGER || next.value().GetType() == TokenType::HEX_INTEGER){
			////testPrint("3");
			//todo:提取这个数字字面量到栈顶
			_instructions_s[_functions_nums].emplace_back(std::string(std::to_string(Getnos())+"    "+"ipush"+" "+next.value().GetValueString()));
			//_instructions.emplace_back(Operation::ipush,atoi(next.value().GetValueString().c_str()),Getnos());
		} else{
		    unreadToken();
            return std::make_optional<CompilationError>(_current_pos,ErrorCode::ErrInvalidPrimaryExpression);
		}
		////testPrint("4");
		return {};
	}

	//<function-call> ::=
	//    <identifier> '(' [<expression-list>] ')'
	std::optional<CompilationError> Analyser::analyseFunctionCall() {
	    //testPrint("analyseFunctionCall");
		//由于就是因为读到了第一个标志符号才进入这个函数的，所以第一个读到的必定是这个函数的名称
		auto next = nextToken();
		auto name = next;

		//'('
		next = nextToken();
		if(!next.has_value() || next.value().GetType() != TokenType::LEFT_BRACKET){
			return std::make_optional<CompilationError>(_current_pos,ErrorCode::ErrInvalidFunctionCall);
		}
		//[<expression-list>]
		//functioncall的时候检查是否和函数定义一致--要考虑
		//预读
		next = nextToken();
		if(next.value().GetType() != RIGHT_BRACKET){
			unreadToken();
			auto err = analyseExpressionList(_functions[name.value().GetValueString()].index);
			if(err.has_value()){
				return err;
			}
		}else {
			unreadToken();
			if(_functions[name.value().GetValueString()].list.params_num != 0){
				return std::make_optional<CompilationError>(_current_pos,ErrorCode::ErrInvalidFunctionCall);
			}
		}
		//')'
		next = nextToken();
		if(!next.has_value() || next.value().GetType() != TokenType::RIGHT_BRACKET){
			return std::make_optional<CompilationError>(_current_pos,ErrorCode::ErrInvalidFunctionCall);
		}
		_instructions_s[_functions_nums].emplace_back(std::string(std::to_string(Getnos())+"    "+"call"+" "+std::to_string(_functions[name.value().GetValueString()].index-1)));
		//_instructions.emplace_back(Operation::Call,_functions[name.value().GetValueString()].index,Getnos());
		return {};
	}

	//<expression-list> ::=
	//    <expression>{','<expression>}
	//需要比对参数列表
	std::optional<CompilationError> Analyser::analyseExpressionList(int no) {
	    //testPrint("analyseExpressionList");
		int count=0;
		auto err = analyseExpression();
		if(err.has_value()){
			return err;
		}
		count++;
		while (true){
			auto next = nextToken();
			if(!next.has_value() || next.value().GetType() != TokenType::COMMA){
				unreadToken();
				break;
			}

			err = analyseExpression();
			if(err.has_value()){
				return err;
			}
			count++;
		}
		if(count != _functions[_functionsTable[no]].list.params_num){
			return std::make_optional<CompilationError>(_current_pos,ErrorCode::ErrIncompleteParametersList);
		}
		return {};
	}

    //<assignment-expression> ::=
    //    <identifier><assignment-operator><expression>
	//语义规则：<assignment-expression>左侧的标识符的值类型必须是可修改的变量（不能是const T、不能是函数名）
	//<assignment-expression>右侧的表达式必须是有值的（不能是void类型、不能是函数名）
	//todo：表达式有可能是无值的吗？
    std::optional<CompilationError> Analyser::analyseAssignmentExpression() {
	    //testPrint("analyseAssignmentExpression");
	    //identifier
	    auto next = nextToken();
	    if(!next.has_value() || next.value().GetType() != TokenType::IDENTIFIER){
	    	unreadToken();
            return std::make_optional<CompilationError>(_current_pos,ErrorCode::ErrNeedIdentifier);
	    }
	    //判断identifier的类型
		//1。不能是const T、不能是函数
		//2。不能是void类型、不能是函数名
        //假设当前函数和全局函数都找不到这个标志服就返回
        std::pair<int32_t ,int32_t > pos;
        if(_constants[_functions_nums].find(next.value().GetValueString()) != _constants[_functions_nums].end()){
            if( _constants[_functions_nums][next.value().GetValueString()].isConst){
                return std::make_optional<CompilationError>(_current_pos,ErrorCode::ErrAssignToConstant);
            }
            if( _constants[_functions_nums][next.value().GetValueString()].type == VOID){
                return std::make_optional<CompilationError>(_current_pos,ErrorCode::ErrAssignToVoid);
            }
            pos = _constants[_functions_nums][next.value().GetValueString()].Index;
        } else if(_constants[0].find(next.value().GetValueString())!= _constants[0].end()){
            if( _constants[0][next.value().GetValueString()].isConst){
                return std::make_optional<CompilationError>(_current_pos,ErrorCode::ErrAssignToConstant);
            }
            if( _constants[0][next.value().GetValueString()].type == VOID){
                return std::make_optional<CompilationError>(_current_pos,ErrorCode::ErrAssignToVoid);
            }
            //testPrint("全局");
            pos = _constants[0][next.value().GetValueString()].Index;
        } else{
            unreadToken();
            return std::make_optional<CompilationError>(_current_pos,ErrorCode::ErrInvalidIdentifier);
        }
        //instructor
		//testPrint("loada");
		//testPrint(next.value().GetValueString());
	//	std::cout<<pos.first<<" "<<pos.second<<std::endl;
	//	std::cout<<_functions_nums<<std::endl;
	//	std::cout<<_instructions_s.size()<<std::endl;
	//	std::cout<<"size:"<<_instructions_s[_functions_nums].size()<<std::endl;
		_instructions_s[_functions_nums].emplace_back(std::string(std::to_string(Getnos())+"    "+"loada"+" "+std::to_string(pos.first)+", "+std::to_string(pos.second)));
	//	std::cout<<"size:"<<_instructions_s[_functions_nums].size()<<std::endl;
	    //assignment-op
		auto name = next;
        next = nextToken();
        if(!next.has_value() || next.value().GetType()!=TokenType::EQUAL_SIGN){
        	unreadToken();
			return std::make_optional<CompilationError>(_current_pos,ErrorCode::ErrInvalidAssignment);
        }

        //expression
		auto err = analyseExpression();
        if(err.has_value()){
			return err;
        }
        //指令
		_instructions_s[_functions_nums].emplace_back(std::string(std::to_string(Getnos())+"    "+"istore"));
      //  _instructions.emplace_back(Operation::istore,Getnos());
		return {};
	}

	//<jump-statement> ::= <return-statement>
	//<return-statement> ::= 'return' [<expression>] ';'
	std::optional<CompilationError> Analyser::analyseJumpStatement() {
	    //testPrint("analyseJumpStatement");
		//'return'
		auto next = nextToken();
		if(!next.has_value() || next.value().GetType() != RETURN){
			return std::make_optional<CompilationError>(_current_pos,ErrorCode::ErrInvalidReturnStatement);
		}

		//[expression]
		//此时需要判断返回的类型是否和正在分析的函数的retvalue一样
		TokenType  type = _functions[_functionsTable[_functions_nums]].retType;
		next = nextToken();
		if(!next.has_value()){
			return std::make_optional<CompilationError>(_current_pos,ErrorCode::ErrEOF);
		}
		if(type == VOID){
			if(next.value().GetType() == SEMICOLON){
				//instruction
				_instructions_s[_functions_nums].emplace_back(std::string(std::to_string(Getnos())+"    "+"ret"));
				return {};
			} else{
				unreadToken();
				return std::make_optional<CompilationError>(_current_pos,ErrorCode::ErrInvalidReturnStatement);
			}
		} else{
			unreadToken();
			auto err = analyseExpression();
			if(err.has_value()){
				return std::make_optional<CompilationError>(_current_pos,ErrorCode::ErrInvalidReturnStatement);
			}
		}
		//';'
		next = nextToken();
		if(!next.has_value() || next.value().GetType() != SEMICOLON){
			return std::make_optional<CompilationError>(_current_pos,ErrorCode::ErrNoSemicolon);
		}
		//指令
		_instructions_s[_functions_nums].emplace_back(std::string(std::to_string(Getnos())+"    "+"iret"));
		//_instructions.emplace_back(Operation::iret,Getnos());
		return {};
	}
	//<scan-statement>  ::= 'scan' '(' <identifier> ')' ';'
	std::optional<CompilationError> Analyser::analyseScanStatement() {
	    //testPrint("analyseScanStatement");
		auto next = nextToken();
		if(!next.has_value() || next.value().GetType() != SCAN){
			return std::make_optional<CompilationError>(_current_pos,ErrorCode::ErrInvalidScanStatement);
		}
		//'('
		next = nextToken();
		if(!next.has_value() || next.value().GetType() != LEFT_BRACKET){
			return std::make_optional<CompilationError>(_current_pos,ErrorCode::ErrInvalidScanStatement);
		}
		//'identifier'
		next = nextToken();
		if(!next.has_value() || next.value().GetType() != IDENTIFIER){
			return std::make_optional<CompilationError>(_current_pos,ErrorCode::ErrInvalidScanStatement);
		}
		//查看是否定义过
		std::pair<int32_t ,int32_t > pos;
		if(_constants[_functions_nums].find(next.value().GetValueString()) != _constants[_functions_nums].end()){
			if( _constants[_functions_nums][next.value().GetValueString()].isConst){
				return std::make_optional<CompilationError>(_current_pos,ErrorCode::ErrAssignToConstant);
			}
			if( _constants[_functions_nums][next.value().GetValueString()].type == VOID){
				return std::make_optional<CompilationError>(_current_pos,ErrorCode::ErrAssignToVoid);
			}
			pos = _constants[_functions_nums][next.value().GetValueString()].Index;
		} else if(_constants[0].find(next.value().GetValueString())!= _constants[0].end()){
			if( _constants[0][next.value().GetValueString()].isConst){
				return std::make_optional<CompilationError>(_current_pos,ErrorCode::ErrAssignToConstant);
			}
			if( _constants[0][next.value().GetValueString()].type == VOID){
				return std::make_optional<CompilationError>(_current_pos,ErrorCode::ErrAssignToVoid);
			}
			pos = _constants[0][next.value().GetValueString()].Index;
		} else{
			unreadToken();
			return std::make_optional<CompilationError>(_current_pos,ErrorCode::ErrInvalidIdentifier);
		}
		//')'
		next = nextToken();
		if(!next.has_value() || next.value().GetType() != RIGHT_BRACKET){
			return std::make_optional<CompilationError>(_current_pos,ErrorCode::ErrInvalidScanStatement);
		}
		//';'
		next = nextToken();
		if(!next.has_value() || next.value().GetType() != SEMICOLON){
			return std::make_optional<CompilationError>(_current_pos,ErrorCode::ErrInvalidScanStatement);
		}

		//instruction
		//testPrint("loada");
		//testPrint(next.value().GetValueString());
	//	std::cout<<_constants[_functions_nums][next.value().GetValueString()].Index.first<<" "<<_constants[_functions_nums][next.value().GetValueString()].Index.second<<std::endl;
		_instructions_s[_functions_nums].emplace_back(std::string(std::to_string(Getnos())+"    "+"loada"+" "+std::to_string(pos.first)+", "+std::to_string(pos.second)));
		//_instructions.emplace_back(Operation::loada,pos,Getnos());
		_instructions_s[_functions_nums].emplace_back(std::string(std::to_string(Getnos())+"    "+"iscan"));
		_instructions_s[_functions_nums].emplace_back(std::string(std::to_string(Getnos())+"    "+"istore"));
		//_instructions.emplace_back(Operation::iscan,Getnos());
		return {};
	}
	//<print-statement> ::= 'print' '(' [<printable-list>] ')' ';'
	std::optional<CompilationError> Analyser::analysePrintStatement() {
	    //testPrint("analysePrintStatement");
		//'print'
		auto next = nextToken();
		if(!next.has_value() || next.value().GetType() != PRINT){
			return std::make_optional<CompilationError>(_current_pos,ErrorCode::ErrInvalidPrint);
		}
		//'('
		next = nextToken();
		if(!next.has_value() || next.value().GetType() != LEFT_BRACKET){
			return std::make_optional<CompilationError>(_current_pos,ErrorCode::ErrInvalidPrint);
		}
		//'printable-list'
		auto err = analysePrintableList();
		if(err.has_value()){
			return err;
		}
		//')'
		next = nextToken();
		if(!next.has_value() || next.value().GetType() != RIGHT_BRACKET){
			return std::make_optional<CompilationError>(_current_pos,ErrorCode::ErrInvalidPrint);
		}
		//';'
		next = nextToken();
		if(!next.has_value() || next.value().GetType() != SEMICOLON){
			return std::make_optional<CompilationError>(_current_pos,ErrorCode::ErrInvalidPrint);
		}
		//instruction
		_instructions_s[_functions_nums].emplace_back(std::string(std::to_string(Getnos())+"    "+"printl"));
		//_instructions.emplace_back(Operation::printl,Getnos());
		return {};
	}
	//<printable-list>  ::= <printable> {',' <printable>}
	//<printable> ::= <expression>
	std::optional<CompilationError> Analyser::analysePrintableList() {
	    //testPrint("analysePrintableList");
		//'printable'
		auto err = analyseExpression();
		if(err.has_value()){
			return err;
		}
		//instruction
		_instructions_s[_functions_nums].emplace_back(std::string(std::to_string(Getnos())+"    "+"iprint"));
		//_instructions.emplace_back(Operation::iprint,Getnos());
		//预读
		while (true){
			auto next = nextToken();
			if(!next.has_value() || next.value().GetType() != COMMA){
				unreadToken();
				return {};
			}

			//输出空格
			_instructions_s[_functions_nums].emplace_back(std::string(std::to_string(Getnos())+"    "+"bipush 32"));
			_instructions_s[_functions_nums].emplace_back(std::string(std::to_string(Getnos())+"    "+"cprint"));

			err = analyseExpression();
			if(err.has_value()){
				return err;
			}
			//instruction
			_instructions_s[_functions_nums].emplace_back(std::string(std::to_string(Getnos())+"    "+"iprint"));
			//_instructions.emplace_back(Operation::iprint,Getnos());
		}
		return {};
	}

	std::optional<Token> Analyser::nextToken() {

		if (_offset == _tokens.size())
			return {};
		// 考虑到 _tokens[0..._offset-1] 已经被分析过了
		// 所以我们选择 _tokens[0..._offset-1] 的 EndPos 作为当前位置
		_current_pos = _tokens[_offset].GetEndPos();
		//std::cout<<"token: "<<_tokens[_offset].GetValueString()<<"Tokenttype: "<<_tokens[_offset].GetType()<<std::endl;
		return _tokens[_offset++];
	}

	void Analyser::unreadToken() {
		if (_offset == 0)
			DieAndPrint("analyser unreads token from the begining.");
		_current_pos = _tokens[_offset - 1].GetEndPos();
		_offset--;
	}

	void Analyser::_add(const Token& name, TokenType value_type,bool flag, bool isConst){
		//std::cout<<"varadd"<<std::endl;
		int step;
		if(_functions_nums == 0){//说明当前没有分析到任何一个函数
			step = 1;
		} else{
			step = 0;
		}
		std::pair<int32_t ,int32_t> Index = std::make_pair(step,_nextTokenIndex);
	//	std::cout<<Index.first<<" "<<Index.second<<std::endl;
		dataType data_ = dataType(Index,value_type,flag,isConst);
		_constants[_functions_nums][name.GetValueString()] = data_;
		//testPrint("dataInfo");
	//	std::cout<<_constants[_functions_nums][name.GetValueString()].Index.first<<" "<<_constants[_functions_nums][name.GetValueString()].Index.second<<std::endl;
		//为添加进去的常量在栈里面留存空间
		int slots;
		if(value_type == INT || value_type == CHAR ){
			slots = 1;
		} else if(value_type == DOUBLE){
			slots = 2;
		} else{
			DieAndPrint("s.type is wrong.");
		}
		_nextTokenIndex += slots;
	}


	void Analyser::_add_function(std::string name,TokenType type){
		//todo:check paramList&
		int index = _functions_nums;
		functionType ft = functionType(index,type);
		_functions[name] = ft;
		_functionsTable.push_back(name);
		std::map<std::string, dataType> map0 = std::map<std::string, dataType>();
	//	//testPrint("!!!!!!!!!!!!!!!!!!");
		_constants.push_back(map0 );
		std::vector<std::string> s;
		_instructions_s.emplace_back(s);
		//std::cout<<_constants.size()<<std::endl;
	}

	bool Analyser::isDeclared(const std::string& s) {
		if(_constants[0].find(s) != _constants[0].end() && _functions.find(s) != _functions.end()){
			return true;
		}
		return false;
	}

	bool Analyser::varIsDeclared(const std::string & s) {
		//std::cout<<"000000000"<<std::endl;
		if(_constants[_functions_nums].find(s) != _constants[_functions_nums].end() && strcmp(s.c_str(),_functionsTable[_functions_nums].c_str())!=0){
			return true;
		}
		return false;
	}


	bool Analyser::isFunction(std::string s){
		return _functions.find(s) != _functions.end();
	}


	bool Analyser::istypeSpecifier(TokenType t){
        return t == INT || t == VOID || t == DOUBLE ||t == CHAR||t == VOID;
	};

	bool Analyser::isAdditiveOp(TokenType t){
		if(t == TokenType::PLUS_SIGN ||t == TokenType::MINUS_SIGN){
			return true;
		}
		return false;
	}

	bool Analyser::isMultiplicativeOp(TokenType t){
		if(t == TokenType::MULTIPLICATION_SIGN || t==TokenType::DIVISION_SIGN){
			return true;
		}
		return false;
	}

	bool Analyser::isRelationalOp(TokenType t){
		if(t == TokenType::LESS_SIGN || t == TokenType::LESS_EQUAL_SIGN || t == TokenType::GREATER_SIGN || t == TokenType::GREATER_EQUAL_SIGN
				||t == TokenType::NEQ_SIGN || t == TokenType::DOUBLE_EQUAL_SIGN){
			return true;
		}
		return false;
	}

	void Analyser::addParamList(paramList& list,TokenType t){
		int slots;
		if(t == INT || t == CHAR){
			slots = 1;
		} else if(t == DOUBLE){
			slots =2;
		}
		list.paras_list.push_back(t);
		list.params_num++;
		list.params_size+=slots;
	}

	int Analyser::_instructions_jump(TokenType relationOP){
	    //std::cout<<_instructions.size()<<std::endl;
	   // //testPrint("cmp");
		_instructions_s[_functions_nums].emplace_back(std::string(std::to_string(Getnos())+"    "+"icmp"));
		//_instructions.emplace_back(Operation::icmp,Getnos());
		//std::cout<<_instructions[_instructions.size()-1].GetNo()<<std::endl;
		switch (relationOP){
			case TokenType ::LESS_SIGN:
				_instructions_s[_functions_nums].emplace_back(std::string(std::to_string(Getnos())+"    "+"jge"));
				//_instructions.emplace_back(Operation::jge,0,Getnos());
				break;
			case TokenType ::LESS_EQUAL_SIGN:
				_instructions_s[_functions_nums].emplace_back(std::string(std::to_string(Getnos())+"    "+"jg"));
				//_instructions.emplace_back(Operation::jg,0,Getnos());
				break;
			case TokenType ::DOUBLE_EQUAL_SIGN:
				_instructions_s[_functions_nums].emplace_back(std::string(std::to_string(Getnos())+"    "+"jne"));
			//	_instructions.emplace_back(Operation::jne,0,Getnos());
				break;
			case TokenType ::GREATER_SIGN:
				_instructions_s[_functions_nums].emplace_back(std::string(std::to_string(Getnos())+"    "+"jle"));
				//_instructions.emplace_back(Operation::jle,0,Getnos());
				break;
			case TokenType ::GREATER_EQUAL_SIGN:
				_instructions_s[_functions_nums].emplace_back(std::string(std::to_string(Getnos())+"    "+"jl"));
				//_instructions.emplace_back(Operation::jl,0,Getnos());
				break;
			case TokenType ::NEQ_SIGN:
				_instructions_s[_functions_nums].emplace_back(std::string(std::to_string(Getnos())+"    "+"je"));
				//_instructions.emplace_back(Operation::je,0,Getnos());
				break;
		}
		return _instructions_s[_functions_nums].size()-1;
	}

    void Analyser::outputInstructions(){
        _output<<".constants:"<<std::endl;
        std::map<std::string,functionType>::iterator iter;
        int n = 0;
        for(iter = _functions.begin();iter != _functions.end();iter++){
            _output<<n++<<" S "<<"\""<<iter->first<<"\""<<std::endl;
        }
        _output<<".start:"<<std::endl;
        for(int j = 0;j<_instructions_s[0].size();j++){
            _output << _instructions_s[0][j]<<std::endl;
        }
        _output<<".functions:"<<std::endl;
        for(int i=1;i<_functions_nums;i++){
            _output<<i-1<<" "<<i-1<<" "<<_functions[_functionsTable[i]].list.params_size<<" 1"<<std::endl;
        }
        for(int i = 1;i<_functions_nums;i++){
            _output<<".F"<<i-1<<":"<<std::endl;
            for(int j = 0;j<_instructions_s[i].size();j++){
                _output << _instructions_s[i][j]<<std::endl;
            }
        }

	}

	int Analyser::getX(int X[]){
		int sum;
		sum= 0;
		_output<<"xi: "<<X[0]<<std::endl;
		for(int i = 0;X[i] != -1;i++){
			sum*=10;
			sum+=X[i];
		}
		return sum;
	}
	void Analyser::outputOP(std::string s){
	//	std::cout<<"????"<<std::endl;
		//_output<<"1";

		int begin =0 ,end = 0,i,n,j;
		for(i = 0;i<s.size();i++){
			if(isalpha(s[i]) && begin == 0){
				begin = i;
			}//the first alpha
			if(!isalpha(s[i]) && begin != 0 && end == 0){
				end = i-1;
				break;
			}//the end of op
		}
		if(end == 0){
			end = s.size()-1;
		}
		char ops[10];
		int x[100];
		for(i = 0,j = begin;j<=end;i++,j++){
			ops[i] = s[j];
		}
		ops[i] = '\0';//取出指令,根据指令类型取出操作数
		memset(x,-1,100*sizeof(int));
	//	_output<<"begin: "<<begin<<"end: "<<end<<std::endl;
	//	_output<<"op: "<<ops<<std::endl;
		if(strcmp(ops,"ipush") == 0){
			_output << fmt::format("{:08b}",2);
			j = 0;
			end = end+2;
			while (end<s.size()){
				x[j++] = s[end++]-'0';
			}
			j = getX(x);
	//		_output<<"x: "<<j<<std::endl;
			_output<<fmt::format("{:032b}",j);
		} else if(strcmp(ops,"loada") == 0){
			_output << fmt::format("{:08b}",10);
			//层次 16bit
			end = end+2;
			j = 0;
			//_output<<"???:"<<s[end-1]<<s[end]<<s[end+1]<<std::endl;
			do {
				x[j++] = s[end++]-'0';
			//	_output<<"s[end] "<<s[end-1]<<"s[end]-'0'"<<s[end-1]-'0'<<" ";
			//	_output<<"x[i] "<<x[j]<<" ";
			}while (isdigit(s[end]));
			n = getX(x);//layout
			j=0;
			end = end+2;
			memset(x,-1,100* sizeof(int));
			while (end<s.size()){
				x[j++] = s[end++]-'0';
				//_output<<"s[end] "<<s[end-1]<<"s[end]-'0'"<<s[end-1]-'0'<<" ";
				//_output<<"x[i] "<<x[j]<<" ";
			}
			j = getX(x);
			//_output<<"layout: "<<n<<std::endl;
			//_output<<"x: "<<j<<std::endl;
			_output<<fmt::format("{:016b}",n);//layout
			_output<<fmt::format("{:032b}",j);//index
		} else if(strcmp(ops,"iload") == 0){
			_output << fmt::format("{:08b}",16);
		} else if(strcmp(ops,"istore") == 0){
			_output << fmt::format("{:08b}",32);
		} else if(strcmp(ops,"iadd") == 0){
			_output << fmt::format("{:08b}",48);
		} else if(strcmp(ops,"isub") == 0){
			_output << fmt::format("{:08b}",52);
		} else if(strcmp(ops,"imul") == 0){
			_output << fmt::format("{:08b}",56);
		} else if(strcmp(ops,"idiv") == 0){
			_output << fmt::format("{:08b}",60);
		} else if(strcmp(ops,"ineg") == 0){
			_output << fmt::format("{:08b}",64);
		} else if(strcmp(ops,"icmp") == 0){
			_output << fmt::format("{:08b}",68);
		} else if(strcmp(ops,"jmp") == 0){
			_output << fmt::format("{:08b}",112);//0x70
			j = 0;
			end = end+2;
			while (end<s.size()){
				x[j++] = s[end++]-'0';
			}
			j = getX(x);
			//_output<<"x: "<<j<<std::endl;
			_output<<fmt::format("{:016b}",j);

		} else if(strcmp(ops,"je") == 0){
			_output << fmt::format("{:08b}",113);
			j = 0;
			end = end+2;
			while (end<s.size()){
				x[j++] = s[end++]-'0';
			}
			j = getX(x);
			_output<<fmt::format("{:016b}",j);
		} else if(strcmp(ops,"jne") == 0){
			_output << fmt::format("{:08b}",114);
			j = 0;
			end = end+2;
			while (end<s.size()){
				x[j++] = s[end++]-'0';
			}
			j = getX(x);
			_output<<fmt::format("{:016b}",j);
		} else if(strcmp(ops,"jl") == 0){
			_output << fmt::format("{:08b}",115);
			j = 0;
			end = end+2;
			while (end<s.size()){
				x[j++] = s[end++]-'0';
			}
			j = getX(x);
			_output<<fmt::format("{:016b}",j);
		} else if(strcmp(ops,"jge") == 0){
			_output << fmt::format("{:08b}",116);
			j = 0;
			end = end+2;
			while (end<s.size()){
				x[j++] = s[end++]-'0';
			}
			j = getX(x);
			_output<<fmt::format("{:016b}",j);
		} else if(strcmp(ops,"jg") == 0){
			_output << fmt::format("{:08b}",117);
			j = 0;
			end = end+2;
			while (end<s.size()){
				x[j++] = s[end++]-'0';
			}
			j = getX(x);
			_output<<fmt::format("{:016b}",j);
		} else if(strcmp(ops,"jle") == 0){
			_output << fmt::format("{:08b}",118);
			j = 0;
			end = end+2;
			while (end<s.size()){
				x[j++] = s[end++]-'0';
			}
			j = getX(x);
			_output<<fmt::format("{:016b}",j);
		} else if(strcmp(ops,"call") == 0){
			_output << fmt::format("{:08b}",128);//0x80
			j = 0;
			end = end+2;
			while (end<s.size()){
				x[j++] = s[end++]-'0';
			}
			j = getX(x);
			_output<<fmt::format("{:016b}",j);
		} else if(strcmp(ops,"ret") == 0){
			_output << fmt::format("{:08b}",136);
		} else if(strcmp(ops,"iret") == 0){
			_output << fmt::format("{:08b}",137);
		} else if(strcmp(ops,"iprint") == 0){
			_output << fmt::format("{:08b}",160);//0xa0
			j = 0;
			end = end+2;
			while (end<s.size()){
				x[j++] = s[end++]-'0';
			}
			j = getX(x);
			_output<<fmt::format("{:032b}",j);
		} else if(strcmp(ops,"pirntl") == 0){
			_output << fmt::format("{:08b}",175);
		} else if(strcmp(ops,"iscan") == 0){
			_output << fmt::format("{:08b}",176);//0xb0
			j = 0;
			end = end+2;
			while (end<s.size()){
				x[j++] = s[end++]-'0';
			}
			j = getX(x);
			_output<<fmt::format("{:032b}",j);
		}

	}


}