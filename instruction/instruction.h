#pragma once

#include <cstdint>
#include <utility>

namespace miniplc0 {

	enum Operation {
	    ILL,
		//内存操作指令
		nop,
		bipush,
		ipush,
		popN,//?
		dup,
		dup2,
		loadc,
		loada,
		New,
		snew,
		iload,
		istore,
		//算数操作
		iadd,
		isub,
		imul,
		idiv,
		ineg,
		icmp,
		//类型转化
		i2d,
		d2i,
		i2c,
		//转移控制
		jmp,
		je,
		jne,
		jl,
		jge,
		jg,
		jle,
		Call,
		ret,
		iret,
		//辅助功能
		iprint,
		dprint,
        cprint,
		sprint,
		printl,
		iscan,
		dscan,
		cscan
	};
	
	class Instruction final {
	private:
		using int32_t = std::int32_t;
	public:
		friend void swap(Instruction& lhs, Instruction& rhs);
	public:
		Instruction(Operation opr, int32_t x,int32_t no) : _opr(opr), _x(x) ,_no(no){
			std::cout<<"instructiton"<<std::endl;
			std::cout<<"x: "<<x<<"no: "<<no<<"_no "<<_no<<std::endl;
			std::cout<<"_x: "<<_x<<"_no"<<_no<<std::endl;
		}
		Instruction(Operation opr,int32_t no):_opr(opr),_no(no){
			std::cout<<"instructiton"<<std::endl;
			std::cout<<"no: "<<no<<"_no "<<_no<<std::endl;
			std::cout<<"_no"<<_no<<std::endl;
		}
		Instruction(Operation opr,std::pair<int32_t ,int32_t > pos,int32_t no){
			_opr = opr;
			_pos = pos;
			_no = no;
		}

		Instruction() : Instruction(Operation::ILL, 0){}

		Instruction(const Instruction& i) { _opr = i._opr; _x = i._x; }
		Instruction(Instruction&& i) :Instruction() { swap(*this, i); }
		Instruction& operator=(Instruction i) { swap(*this, i); return *this; }
		bool operator==(const Instruction& i) const { return _opr == i._opr && _x == i._x; }

		Operation GetOperation() const { return _opr; }
		int32_t GetX() const {
			//std::cout<<"no "<<_no<<"pos "<<_pos.first<<" "<<_pos.second<<std::endl;
			return _x; }
		void setX(int x) {
			_x = x;
		}

		std::pair<int32_t ,int32_t > GetPos()const{
            return _pos;
		};

		std::int32_t GetNo() const{
			std::cout<<"printno"<<_no<<std::endl;
			return _no;
		}
	private:
		Operation _opr;
		int32_t _x;
		std::pair<int32_t ,int32_t > _pos;
		int32_t _no;
	};

	inline void swap(Instruction& lhs, Instruction& rhs) {
		using std::swap;
		swap(lhs._opr, rhs._opr);
		swap(lhs._x, rhs._x);
	}
}