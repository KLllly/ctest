#include "tokenizer/tokenizer.h"

#include <cctype>
#include <sstream>
#include <math.h>
#include <string>



namespace miniplc0 {

	std::pair<std::optional<Token>, std::optional<CompilationError>> Tokenizer::NextToken() {
		if (!_initialized)
			readAll();
		if (_rdr.bad())
			return std::make_pair(std::optional<Token>(), std::make_optional<CompilationError>(0, 0, ErrorCode::ErrStreamError));
		if (isEOF())
			return std::make_pair(std::optional<Token>(), std::make_optional<CompilationError>(0, 0, ErrorCode::ErrEOF));
		auto p = nextToken();
		if (p.second.has_value())
			return std::make_pair(p.first, p.second);
		auto err = checkToken(p.first.value());
		if (err.has_value())
			return std::make_pair(p.first, err.value());
		return std::make_pair(p.first, std::optional<CompilationError>());
	}

	std::pair<std::vector<Token>, std::optional<CompilationError>> Tokenizer::AllTokens() {
		std::vector<Token> result;
		while (true) {
			auto p = NextToken();
			if (p.second.has_value()) {
				if (p.second.value().GetCode() == ErrorCode::ErrEOF)
					return std::make_pair(result, std::optional<CompilationError>());
				else
					return std::make_pair(std::vector<Token>(), p.second);
			}
			result.emplace_back(p.first.value());
		}
	}

	// 注意：这里的返回值中 Token 和 CompilationError 只能返回一个，不能同时返回。
    std::pair<std::optional<Token>, std::optional<CompilationError>> Tokenizer::nextToken() {
        std::stringstream ss;
        std::pair<std::optional<Token>, std::optional<CompilationError>> result;
        std::pair<int64_t, int64_t> pos;// <行号，列号>，表示当前token的第一个字符在源代码中的位置
        DFAState current_state = DFAState::INITIAL_STATE;
        // 这是一个死循环，除非主动跳出
        while (true) {
            // 读一个字符，请注意auto推导得出的类型是std::optional<char>
            auto current_char = nextChar();

            // 针对当前的状态进行不同的操作
            switch (current_state) {
                case INITIAL_STATE: {
                    // 已经读到了文件尾
                    if (!current_char.has_value())
                        // 返回一个空的token，和编译错误ErrEOF：遇到了文件尾
                        return std::make_pair(std::optional<Token>(), std::make_optional<CompilationError>(0, 0, ErrEOF));
                    // 获取读到的字符的值，注意auto推导出的类型是char
                    auto ch = current_char.value();
                    // 标记是否读到了不合法的字符，初始化为否
                    auto invalid = false;

                    if (miniplc0::isspace(ch)) // 是否为空格(' ')、水平定位字符
                        //('\t')、归位键('\r')、换行('\n')、垂直定位字符('\v')或翻页('\f')的情况
                        current_state = DFAState::INITIAL_STATE; // 保留当前状态为初始状态，此处直接break也是可以的
                    else if (!miniplc0::isprint(ch)) // control codes and backspace
                        //isprint:看ch是否是可打印字符
                        invalid = true;
                    else if (miniplc0::isdigit(ch)){// 读到的字符是数字,需要判断是十进制还是十六进制还是浮点数
                        if(ch == '0'){
                            current_char = nextChar();//预读一个字符
                            //这里由于预读了字符，即使回退后也无法使得currentchar回到上一个字符，所以直接在里面存入ss，并且直接break
                            if(!current_char.has_value()){
                                //当前读到文件末尾，直接解析0为十进制无符号整数
                                unreadLast();
                                //todo:fix the column
                                //直接返回之前要清空ss
                                ss>>ch;
                                return std::make_pair(std::make_optional<Token>(TokenType :: UNSIGNED_INTEGER, 0 ,previousPos(), currentPos()),std::optional<CompilationError>());
                            }
                            ch = current_char.value();
                            ss<<'0';
                            pos = previousPos();
                            if(ch == 'x'||ch == 'X'){
                                //不必回退这个字符，直接进入十六进制的时候就是没有前缀的
                                //存储的时候还是要加入0x，便于区分
                                ss<<ch;
                                current_state = DFAState :: HEXDIGIT_STATE;
                                break;
                            } else if(isdigit(ch)){
                                unreadLast();
                                current_state = DFAState :: UNSIGNED_INTEGER_STATE;
                                break;
                            } else{
                                unreadLast();
                                ss>>ch;
                                return std::make_pair(std::make_optional<Token>(TokenType :: UNSIGNED_INTEGER, 0 ,previousPos(), currentPos()),std::optional<CompilationError>());
                            }
                        } else{
                            //开头不是0那么就不可能是十六进制，但还是有可能是浮点数，先全部转换为十进制
                            current_state = DFAState::UNSIGNED_INTEGER_STATE;
                        }
                    }
                    else if (miniplc0::isalpha(ch)) // 读到的字符是英文字母
                        current_state = DFAState::IDENTIFIER_STATE; // 切换到标识符的状态
                    else {
                        switch (ch) {
                            //todo:fix the column
                            case '=':
                                current_state = EQUAL_SIGN_STATE;
                                break;
                            case '-':
                                return std::make_pair(std::make_optional<Token>(TokenType::MINUS_SIGN, '-', previousPos(), currentPos()),std::optional<CompilationError>());
                            case '+':
                                return std::make_pair(std::make_optional<Token>(TokenType::PLUS_SIGN, '+', previousPos(), currentPos()),std::optional<CompilationError>());
                            case '*':
                                return std::make_pair(std::make_optional<Token>(TokenType::MULTIPLICATION_SIGN,'*',pos,currentPos()),std::optional<CompilationError>());
                            case '/':
                                current_state = SLASH_STATE;
                                break;
                            case ';':
                                return std::make_pair(std::make_optional<Token>(TokenType::SEMICOLON, ';', previousPos(), currentPos()),std::optional<CompilationError>());
                            case '(':
                                return std::make_pair(std::make_optional<Token>(TokenType::LEFT_BRACKET, '(', previousPos(), currentPos()),std::optional<CompilationError>());
                            case ')':
                                return std::make_pair(std::make_optional<Token>(TokenType::RIGHT_BRACKET, ')', previousPos(), currentPos()),std::optional<CompilationError>());
                            case '[':
                                return std::make_pair(std::make_optional<Token>(TokenType::LEFT_BRACK, '[', previousPos(), currentPos()),std::optional<CompilationError>());
                            case ']':
                                return std::make_pair(std::make_optional<Token>(TokenType::RIGHT_BRACK, ']', previousPos(), currentPos()),std::optional<CompilationError>());
                            case '{':
                                return std::make_pair(std::make_optional<Token>(TokenType::LEFT_BRACE, '{', previousPos(), currentPos()),std::optional<CompilationError>());
                            case '}':
                                return std::make_pair(std::make_optional<Token>(TokenType::RIGHT_BRACE, '}', previousPos(), currentPos()),std::optional<CompilationError>());
                            case '<':
                                current_state = LESS_SIGN_STATE;
                                break;
                            case '>':
                                current_state = GREATER_SIGN_STATE;
                                break;
                            case '.':
                                return std::make_pair(std::make_optional<Token>(TokenType::POINT_SIGN, '.', previousPos(), currentPos()),std::optional<CompilationError>());
                            case ',':
                                return std::make_pair(std::make_optional<Token>(TokenType::COMMA, ',', previousPos(), currentPos()),std::optional<CompilationError>());
                            case ':':
                                return std::make_pair(std::make_optional<Token>(TokenType::COLON, ':', previousPos(), currentPos()),std::optional<CompilationError>());
                            case '!':
                                current_state = EXCLAMATION_SIGN_STATE;
                                break;
                            case '?':
                                return std::make_pair(std::make_optional<Token>(TokenType::QUESTION_SIGN, '?', previousPos(), currentPos()),std::optional<CompilationError>());
                            case '%':
                                return std::make_pair(std::make_optional<Token>(TokenType::PERCENTAGE_SIGN, '%', previousPos(), currentPos()),std::optional<CompilationError>());
                            case '^':
                                return std::make_pair(std::make_optional<Token>(TokenType::SQUARE_SIGN, '^', previousPos(), currentPos()),std::optional<CompilationError>());
                            case '&':
                                return std::make_pair(std::make_optional<Token>(TokenType::AND_SIGN, '&', previousPos(), currentPos()),std::optional<CompilationError>());
                            case '|':
                                return std::make_pair(std::make_optional<Token>(TokenType::OR_SIGN, '|', previousPos(), currentPos()),std::optional<CompilationError>());
                            case '~':
                                return std::make_pair(std::make_optional<Token>(TokenType::NOT_SIGN, '~', previousPos(), currentPos()),std::optional<CompilationError>());
                            case '\\':
                                return std::make_pair(std::make_optional<Token>(TokenType::BACKSLASH, '\\', previousPos(), currentPos()),std::optional<CompilationError>());
                            case '\"':
                                return std::make_pair(std::make_optional<Token>(TokenType::DOUBLE_QUOTES, '\"', previousPos(), currentPos()),std::optional<CompilationError>());
                            case '\'':
                                return std::make_pair(std::make_optional<Token>(TokenType::SINGLE_QUOTES, '\'', previousPos(), currentPos()),std::optional<CompilationError>());
                            case '`':
                                return std::make_pair(std::make_optional<Token>(TokenType::MINI_QUOTES, '`', previousPos(), currentPos()),std::optional<CompilationError>());
                            case '$':
                                return std::make_pair(std::make_optional<Token>(TokenType::DOLLAR_SIGN, '$', previousPos(), currentPos()),std::optional<CompilationError>());
                            case '#':
                                return std::make_pair(std::make_optional<Token>(TokenType::HASH_SIGN, '#', previousPos(), currentPos()),std::optional<CompilationError>());
                            case '@':
                                return std::make_pair(std::make_optional<Token>(TokenType::AT_SIGN, '@', previousPos(), currentPos()),std::optional<CompilationError>());
                            case '_':
                                return std::make_pair(std::make_optional<Token>(TokenType::UNDERLINE, '_', previousPos(), currentPos()),std::optional<CompilationError>());

                                // 只要是没有出现在case中的符号都属于不接受的字符，全部进入defalt，这样我们不需要特别设置一个函数来判断有效性
                            default:
                                invalid = true;
                                break;
                        }
                    }
                    // 如果读到的字符导致了状态的转移，说明它是一个token的第一个字符
                    if (current_state != DFAState::INITIAL_STATE)
                        pos = previousPos(); // 记录该字符的的位置为token的开始位置
                    // 读到了不合法的字符
                    if (invalid) {
                        // 回退这个字符
                        unreadLast();
                        // 返回编译错误：非法的输入
                        return std::make_pair(std::optional<Token>(), std::make_optional<CompilationError>(currentPos(), ErrorCode::ErrInvalidInput));
                    }
                    // 如果读到的字符导致了状态的转移，说明它是一个token的第一个字符
                    if (current_state != DFAState::INITIAL_STATE) // ignore white spaces
                        ss << ch; // 存储读到的字符
                    break;
                }

                    // 当前状态是整数字面量--十进制
                    //由于文法里面明确指出了，不能有前导零，没必要处理，只要0后面还有数字那么就报错。
                    //todo：十进制中有可能切换到浮点数
                case UNSIGNED_INTEGER_STATE: {
                    if (!current_char.has_value())
                        // 如果当前已经读到了文件尾，则解析已经读到的字符串为整数
                        //此处注意，我们需要判断这时的整数字符串有没有超出限定的范围
                    {
                        std::string tmp,maxnum;
                        int num;
                        maxnum="2147483647";
                        ss >> tmp;
                        //去除前导零
                        if(tmp[0]=='0'){
                            //也就是说0后面有数字,返回不合法的输入
                            if(tmp.size()>1){
                                return std::make_pair(std::optional<Token>(),std::make_optional<CompilationError>(currentPos(),ErrorCode::ErrInvalidInput));
                            }
                        }
                        if(tmp.size()>=10){
                            if(tmp.size()==10){
                                if(tmp.compare(maxnum)>0){
                                    return std::make_pair(std::optional<Token>(), std::make_optional<CompilationError>(currentPos(), ErrIntegerOverflow));
                                }
                            } else{
                                return std::make_pair(std::optional<Token>(), std::make_optional<CompilationError>(currentPos(), ErrIntegerOverflow));
                            }
                        }

                        num = std::atoi(tmp.c_str());
                        return std::make_pair(std::make_optional<Token>(TokenType :: UNSIGNED_INTEGER, num, pos, currentPos()),std::optional<CompilationError>());
                    }
                    // 获取读到的字符的值，注意auto推导出的类型是char
                    auto ch = current_char.value();

                    //如果读到的字符是数字，则存储读到的字符
                    if(miniplc0::isdigit(ch)){
                        ss<<ch;
                    } else if(miniplc0::isalpha(ch)){
                        //如果读到的是字母，则存储读到的字符，并切换状态到标识符
                        ss<<ch;
                        current_state = DFAState ::IDENTIFIER_STATE;
                    } else {
                        //则回退读到的字符，并解析已经读到的字符串为整数
                        unreadLast();
                        std::string tmp,maxnum;
                        int num;
                        maxnum="2147483647";
                        ss >> tmp;

                        if(tmp[0]=='0'){
                            //也就是说0后面有数字,返回不合法的输入
                            if(tmp.size()>1){
                                return std::make_pair(std::optional<Token>(),std::make_optional<CompilationError>(currentPos(),ErrorCode::ErrInvalidInput));
                            }
                        }
                        //这里是为了判断是否越界
                        if(tmp.size()>=10){
                            if(tmp.size()==10){
                                if(tmp.compare(maxnum)>0){
                                    return std::make_pair(std::optional<Token>(), std::make_optional<CompilationError>(currentPos(), ErrIntegerOverflow));
                                }
                            } else{
                                return std::make_pair(std::optional<Token>(), std::make_optional<CompilationError>(currentPos(), ErrIntegerOverflow));
                            }
                        }

                        num = std::atoi(tmp.c_str());
                        return std::make_pair(std::make_optional<Token>(TokenType :: UNSIGNED_INTEGER, num, pos, currentPos()),std::optional<CompilationError>());
                    }
                    break;
                }
                    //十六进制
                case HEXDIGIT_STATE:{
                    if(!current_char.has_value()){
                        std::string tmp,s;
                        ss >> tmp;
                        if(tmp.size()==2){//只有0x
                            return std::make_pair(std::optional<Token>(),std::make_optional<CompilationError>(currentPos(),ErrorCode::ErrInvalidInput));
                        }

                        s = tmp.substr(2,tmp.size()-2);//去掉开头的0x
                        int sum=0,a,b;
                        bool flag = false;
                        for(int i = 0; i<s.size();i++){
                            if(isupper(s[i])){
                                b = s[i]-'A'+10;
                            } else if(islower(s[i])){
                                b = s[i]-'a'+10;
                            } else{//s[i] is digit
                                b = s[i]-'0';
                            }
                            a = sum;
                            sum *= 16;
                            if(sum < a){
                                flag = true;
                                break;
                            }
                            a = sum;
                            sum += b;
                            if(sum<a || sum<b){
                                flag = true;
                                break;
                            }
                        }
                        if(flag){//说明此时溢出了
                            //todo:check it
                            return std::make_pair(std::optional<Token>(),std::make_optional<CompilationError>(currentPos(),ErrorCode::ErrIntegerOverflow));
                        }
                        //用十进制的int型来存储十六进制的数字，便于后面计算
                        return std::make_pair(std::make_optional<Token>(TokenType::HEX_INTEGER,sum,pos,currentPos()),std::optional<CompilationError>());
                    }

                    //如同十进制中我们处理无效标识符一样，这里我们仍然要注意数字开头的标志，所以如果读到非组成部分的字母仍然转去标识符
                    auto ch = current_char.value();
                    if(isHexdigit(ch)){
                        ss << ch;
                    } else if(isalpha(ch)){//除了十六进制符号以外的符号
                        current_state = IDENTIFIER_STATE;
                    } else{
                        unreadLast();
                        std::string tmp,s;
                        ss >> tmp;
                        if(tmp.size()==2){//只有0x
                            return std::make_pair(std::optional<Token>(),std::make_optional<CompilationError>(currentPos(),ErrorCode::ErrInvalidInput));
                        }

                        s = tmp.substr(2,tmp.size()-2);//去掉开头的0x
                        int sum=0,a,b;
                        bool flag = false;
                        for(int i = 0; i<s.size();i++){
                            if(isupper(s[i])){
                                b = s[i]-'A'+10;
                            } else if(islower(s[i])){
                                b = s[i]-'a'+10;
                            } else{//s[i] is digit
                                b = s[i]-'0';
                            }
                            a = sum;
                            sum *= 16;
                            if(sum < a){
                                flag = true;
                                break;
                            }
                            sum += b;
                            if(sum<a || sum<b){
                                flag = true;
                                break;
                            }
                        }
                        if(flag){//说明此时溢出了
                            //todo:check it
                            return std::make_pair(std::optional<Token>(),std::make_optional<CompilationError>(currentPos(),ErrorCode::ErrIntegerOverflow));
                        }
                        //用十进制的int型来存储十六进制的数字，便于后面计算
                        return std::make_pair(std::make_optional<Token>(TokenType::HEX_INTEGER,sum,pos,currentPos()),std::optional<CompilationError>());
                    }
                    break;
                }
                case IDENTIFIER_STATE: {
                    if (!current_char.has_value())
                        // 如果当前已经读到了文件尾，则解析已经读到的字符串
                        //     如果解析结果是关键字，那么返回对应关键字的token，否则返回标识符的token
                    {
                        std:: string s;
                        ss >> s;
                        if(miniplc0::isdigit(s[0])){
                            //说明是从数字那部分转过来的无效标志
                           // std::cout<<s<<std::endl;
                            return std::make_pair(std::optional<Token>(),std::make_optional<CompilationError>(currentPos(),ErrInvalidIdentifier));
                        }

                        if(s=="const"){
                            return std::make_pair(std::make_optional<Token>(TokenType::CONST,s, pos, currentPos()),std::optional<CompilationError>());
                        } else if(s=="print"){
                            return std::make_pair(std::make_optional<Token>(TokenType::PRINT,s, pos, currentPos()),std::optional<CompilationError>());
                        } else if(s=="void"){
                            return std::make_pair(std::make_optional<Token>(TokenType::VOID,s, pos, currentPos()),std::optional<CompilationError>());
                        }else if(s=="int"){
                            return std::make_pair(std::make_optional<Token>(TokenType::INT,s, pos, currentPos()),std::optional<CompilationError>());
                        }else if(s=="char"){
                            return std::make_pair(std::make_optional<Token>(TokenType::CHAR,s, pos, currentPos()),std::optional<CompilationError>());
                        }else if(s=="double"){
                            return std::make_pair(std::make_optional<Token>(TokenType::DOUBLE,s, pos, currentPos()),std::optional<CompilationError>());
                        }else if(s=="struct"){
                            return std::make_pair(std::make_optional<Token>(TokenType::STRUCT,s, pos, currentPos()),std::optional<CompilationError>());
                        }else if(s=="if"){
                            return std::make_pair(std::make_optional<Token>(TokenType::IF,s, pos, currentPos()),std::optional<CompilationError>());
                        }else if(s=="else"){
                            return std::make_pair(std::make_optional<Token>(TokenType::ELSE,s, pos, currentPos()),std::optional<CompilationError>());
                        }else if(s=="switch"){
                            return std::make_pair(std::make_optional<Token>(TokenType::SWITCH,s, pos, currentPos()),std::optional<CompilationError>());
                        }else if(s=="case"){
                            return std::make_pair(std::make_optional<Token>(TokenType::CASE,s, pos, currentPos()),std::optional<CompilationError>());
                        }else if(s=="default"){
                            return std::make_pair(std::make_optional<Token>(TokenType::DEFAULT,s, pos, currentPos()),std::optional<CompilationError>());
                        }else if(s=="while"){
                            return std::make_pair(std::make_optional<Token>(TokenType::WHILE,s, pos, currentPos()),std::optional<CompilationError>());
                        }else if(s=="for"){
                            return std::make_pair(std::make_optional<Token>(TokenType::FOR,s, pos, currentPos()),std::optional<CompilationError>());
                        }else if(s=="do"){
                            return std::make_pair(std::make_optional<Token>(TokenType::DO,s, pos, currentPos()),std::optional<CompilationError>());
                        }else if(s=="return"){
                            return std::make_pair(std::make_optional<Token>(TokenType::RETURN,s, pos, currentPos()),std::optional<CompilationError>());
                        }else if(s=="break"){
                            return std::make_pair(std::make_optional<Token>(TokenType::BREAK,s, pos, currentPos()),std::optional<CompilationError>());
                        }else if(s=="continue"){
                            return std::make_pair(std::make_optional<Token>(TokenType::CONTINUE,s, pos, currentPos()),std::optional<CompilationError>());
                        }else if(s=="scan"){
                            return std::make_pair(std::make_optional<Token>(TokenType::SCAN,s, pos, currentPos()),std::optional<CompilationError>());
                        } else{
                            return std::make_pair(std::make_optional<Token>(TokenType::IDENTIFIER,s, pos, currentPos()),std::optional<CompilationError>());
                        }
                    }

                    auto ch = current_char.value();
                    // 如果读到的是字符或字母，则存储读到的字符
                    if(miniplc0::isdigit(ch)||miniplc0::isalpha(ch)){
                        ss<<ch;
                    } else{
                        // 如果读到的字符不是上述情况之一，则回退读到的字符，并解析已经读到的字符串
                        //     如果解析结果是关键字，那么返回对应关键字的token，否则返回标识符的token
                        unreadLast();
                        std:: string s;
                        ss >> s;
                        //std::cout<<s<<std::endl;
                        if(miniplc0::isdigit(s[0])){
                            //std::cout<<s<<std::endl;
                            return std::make_pair(std::optional<Token>(),std::make_optional<CompilationError>(currentPos(),ErrInvalidIdentifier));
                        }

                        if(s=="const"){
                            return std::make_pair(std::make_optional<Token>(TokenType::CONST,s, pos, currentPos()),std::optional<CompilationError>());
                        } else if(s=="print"){
                            return std::make_pair(std::make_optional<Token>(TokenType::PRINT,s, pos, currentPos()),std::optional<CompilationError>());
                        } else if(s=="void"){
                            return std::make_pair(std::make_optional<Token>(TokenType::VOID,s, pos, currentPos()),std::optional<CompilationError>());
                        }else if(s=="int"){
                            return std::make_pair(std::make_optional<Token>(TokenType::INT,s, pos, currentPos()),std::optional<CompilationError>());
                        }else if(s=="char"){
                            return std::make_pair(std::make_optional<Token>(TokenType::CHAR,s, pos, currentPos()),std::optional<CompilationError>());
                        }else if(s=="double"){
                            return std::make_pair(std::make_optional<Token>(TokenType::DOUBLE,s, pos, currentPos()),std::optional<CompilationError>());
                        }else if(s=="struct"){
                            return std::make_pair(std::make_optional<Token>(TokenType::STRUCT,s, pos, currentPos()),std::optional<CompilationError>());
                        }else if(s=="if"){
                            return std::make_pair(std::make_optional<Token>(TokenType::IF,s, pos, currentPos()),std::optional<CompilationError>());
                        }else if(s=="else"){
                            return std::make_pair(std::make_optional<Token>(TokenType::ELSE,s, pos, currentPos()),std::optional<CompilationError>());
                        }else if(s=="switch"){
                            return std::make_pair(std::make_optional<Token>(TokenType::SWITCH,s, pos, currentPos()),std::optional<CompilationError>());
                        }else if(s=="case"){
                            return std::make_pair(std::make_optional<Token>(TokenType::CASE,s, pos, currentPos()),std::optional<CompilationError>());
                        }else if(s=="default"){
                            return std::make_pair(std::make_optional<Token>(TokenType::DEFAULT,s, pos, currentPos()),std::optional<CompilationError>());
                        }else if(s=="while"){
                            return std::make_pair(std::make_optional<Token>(TokenType::WHILE,s, pos, currentPos()),std::optional<CompilationError>());
                        }else if(s=="for"){
                            return std::make_pair(std::make_optional<Token>(TokenType::FOR,s, pos, currentPos()),std::optional<CompilationError>());
                        }else if(s=="do"){
                            return std::make_pair(std::make_optional<Token>(TokenType::DO,s, pos, currentPos()),std::optional<CompilationError>());
                        }else if(s=="return"){
                            return std::make_pair(std::make_optional<Token>(TokenType::RETURN,s, pos, currentPos()),std::optional<CompilationError>());
                        }else if(s=="break"){
                            return std::make_pair(std::make_optional<Token>(TokenType::BREAK,s, pos, currentPos()),std::optional<CompilationError>());
                        }else if(s=="continue"){
                            return std::make_pair(std::make_optional<Token>(TokenType::CONTINUE,s, pos, currentPos()),std::optional<CompilationError>());
                        }else if(s=="scan"){
                            return std::make_pair(std::make_optional<Token>(TokenType::SCAN,s, pos, currentPos()),std::optional<CompilationError>());
                        } else{
                            return std::make_pair(std::make_optional<Token>(TokenType::IDENTIFIER,s, pos, currentPos()),std::optional<CompilationError>());
                        }
                    }
                    break;
                }
                    //<
                case LESS_SIGN_STATE:{
                    if(!current_char.has_value()){
                        unreadLast();
                        return std::make_pair(std::make_optional<Token>(TokenType::LESS_SIGN,'<',pos,currentPos()),std::optional<CompilationError>());
                    }

                    auto ch = current_char.value();
                    if(ch == '='){
                        std::string s("<=");
                        return std::make_pair(std::make_optional<Token>(TokenType::LESS_EQUAL_SIGN,s,pos,currentPos()),std::optional<CompilationError>());

                    } else{
                        unreadLast();
                        return std::make_pair(std::make_optional<Token>(TokenType::LESS_SIGN,'<',pos,currentPos()),std::optional<CompilationError>());
                    }
                }

                case GREATER_SIGN_STATE:{
                    if(!current_char.has_value()){
                        unreadLast();
                        return std::make_pair(std::make_optional<Token>(TokenType::GREATER_SIGN,'>',pos,currentPos()),std::optional<CompilationError>());
                    }

                    auto ch = current_char.value();
                    if(ch == '='){
                        std::string s(">=");
                        return std::make_pair(std::make_optional<Token>(TokenType::GREATER_EQUAL_SIGN,s,pos,currentPos()),std::optional<CompilationError>());

                    } else{
                        unreadLast();
                        return std::make_pair(std::make_optional<Token>(TokenType::GREATER_SIGN,'>',pos,currentPos()),std::optional<CompilationError>());
                    }
                }
                case EQUAL_SIGN_STATE:{
                    if(!current_char.has_value()){
                        unreadLast();
                        return std::make_pair(std::make_optional<Token>(TokenType::EQUAL_SIGN,'=',pos,currentPos()),std::optional<CompilationError>());
                    }

                    auto ch = current_char.value();
                    if(ch == '='){
                        std::string s("==");
                        return std::make_pair(std::make_optional<Token>(TokenType::DOUBLE_EQUAL_SIGN,s,pos,currentPos()),std::optional<CompilationError>());

                    } else{
                        unreadLast();
                        return std::make_pair(std::make_optional<Token>(TokenType::EQUAL_SIGN,'=',pos,currentPos()),std::optional<CompilationError>());
                    }
                }
                case EXCLAMATION_SIGN_STATE:{
                    if(!current_char.has_value()){
                        unreadLast();
                        return std::make_pair(std::make_optional<Token>(TokenType::EXCLAMATION_SIGN,'!',pos,currentPos()),std::optional<CompilationError>());
                    }

                    auto ch = current_char.value();
                    if(ch == '='){
                        std::string s("!=");
                        return std::make_pair(std::make_optional<Token>(TokenType::NEQ_SIGN,s,pos,currentPos()),std::optional<CompilationError>());

                    } else{
                        unreadLast();
                        return std::make_pair(std::make_optional<Token>(TokenType::EXCLAMATION_SIGN,'!',pos,currentPos()),std::optional<CompilationError>());
                    }
                }
                case SLASH_STATE:{
                    if(!current_char.has_value()){
                        unreadLast();
                        return std::make_pair(std::make_optional<Token>(TokenType::DIVISION_SIGN,'/',pos,currentPos()),std::optional<CompilationError>());
                    }

                    auto ch = current_char.value();
                    if(ch == '/'){
                       while (ch != '\r' && ch != '\n'){
                           current_char =nextChar();
                           if(!current_char.has_value()){
                               return std::make_pair(std::optional<Token>(),std::make_optional<CompilationError>(currentPos(),ErrEOF));
                           }
                           ch = current_char.value();
                       }
                    } else if(ch == '*'){
                        int keep = 0,count = 0;
                        while (1){
                            count ++;
                            current_char =nextChar();
                            if(!current_char.has_value()){
                                return std::make_pair(std::optional<Token>(),std::make_optional<CompilationError>(currentPos(),ErrEOF));
                            }
                            ch = current_char.value();
                            if(ch == '*'){
                                keep = count;
                            } else if(ch == '/' && keep==count-1 ){
                                current_state = INITIAL_STATE;
                                break;
                            }
                        }
                    } else{
                        unreadLast();
                        return std::make_pair(std::make_optional<Token>(TokenType::DIVISION_SIGN,'/',pos,currentPos()),std::optional<CompilationError>());
                    }
                    break;
                }
                default:
                    DieAndPrint("unhandled state.");
                    break;
            }
        }
        // 预料之外的状态，如果执行到了这里，说明程序异常
        return std::make_pair(std::optional<Token>(), std::optional<CompilationError>());
    }

    bool Tokenizer::isHexdigit(char c){
        if(isdigit(c)||c=='a'||c=='b'||c=='c'||c=='d'||c=='e'||c=='f'
           ||c=='A'||c=='B'||c=='C'||c=='D'||c=='E' ||c=='F'){
            return true;
        }
        return false;
	}
    std::optional<CompilationError> Tokenizer::checkToken(const Token& t) {
		switch (t.GetType()) {
			case IDENTIFIER: {
				auto val = t.GetValueString();
				if (miniplc0::isdigit(val[0]))
					return std::make_optional<CompilationError>(t.GetStartPos().first, t.GetStartPos().second, ErrorCode::ErrInvalidIdentifier);
				break;
			}
		default:
			break;
		}
		return {};
	}

	void Tokenizer::readAll() {
		if (_initialized)
			return;
		for (std::string tp; std::getline(_rdr, tp);)
			_lines_buffer.emplace_back(std::move(tp + "\n"));
		_initialized = true;
		_ptr = std::make_pair<int64_t, int64_t>(0, 0);
		return;
	}

	// Note: We allow this function to return a postion which is out of bound according to the design like std::vector::end().
	std::pair<uint64_t, uint64_t> Tokenizer::nextPos() {
		if (_ptr.first >= _lines_buffer.size())
			DieAndPrint("advance after EOF");
		if (_ptr.second == _lines_buffer[_ptr.first].size() - 1)
			return std::make_pair(_ptr.first + 1, 0);
		else
			return std::make_pair(_ptr.first, _ptr.second + 1);
	}

	std::pair<uint64_t, uint64_t> Tokenizer::currentPos() {
		return _ptr;
	}

	std::pair<uint64_t, uint64_t> Tokenizer::previousPos() {
		if (_ptr.first == 0 && _ptr.second == 0)
			DieAndPrint("previous position from beginning");
		if (_ptr.second == 0)
			return std::make_pair(_ptr.first - 1, _lines_buffer[_ptr.first - 1].size() - 1);
		else
			return std::make_pair(_ptr.first, _ptr.second - 1);
	}

	std::optional<char> Tokenizer::nextChar() {
		if (isEOF())
			return {}; // EOF
		auto result = _lines_buffer[_ptr.first][_ptr.second];
		_ptr = nextPos();
		//std::cout<<result<<std::endl;
		return result;
	}

	bool Tokenizer::isEOF() {
		return _ptr.first >= _lines_buffer.size();
	}

	// Note: Is it evil to unread a buffer?
	void Tokenizer::unreadLast() {
		_ptr = previousPos();
	}
}