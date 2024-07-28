#include <iostream>
#include <vector>    
#include <map>       
#include <string>   
#include <cmath>     
#include <bitset>
#include <sstream>
#include <fstream>
#include <iomanip>
using namespace std;


int MEM[1024]={0};
int R[32]={0};

//1 index represents the EX/MEM registers 
//2 index represents the MEM/WB registers 

int R_ALUOutputReg;
int R_MEMoutdata;
int R_writeBackData;
int R_memWriteData1;
int R_regDst1;
int R_regDst2;

bool zeroFlagGenerated;
bool R_memDo1;
bool R_writeBackDo1;
bool R_regWrite1;
bool R_memRead1;   
bool R_memToReg1;
bool R_memWrite1;
bool R_end1;
bool R_writeBackDo2;
bool R_regWrite2;
bool R_end2;


//IF/ID registers
string R_getOpCode;
string R_Instruction;
int R_PC;

//ID/EX registers
int R_RegDst; 
int R_BranchAddress; 
int R_JumpAddress; 
int R_ALUInput1; 
int R_ALUInput2; 
int R_dataForStore; 
int R_ALUControl;
int R_btype;

int JA; 
int BA;

bool R_Branch;
bool R_MemRead;
bool R_MemtoReg;
bool R_MemWrite;
bool R_Jump;
bool R_RegWrite;
bool R_ALUDo;
bool R_MemDo;
bool R_WritebackDo;
bool R_End;

bool JumpFlag;
bool BranchPossible;
bool ZeroFlag;
bool EndFlag;

map<string,string> opCode;//opCode for different instructions
map<string,string> funct3;//funct3 type for different R-type instructions and branch instructions
map<string,string> funct7;//for R-type
map<string,int> indicesOfLabels;//contains the line index number in the assembly code at which the labels first appeared, needed for branch/jump

ifstream inp("input1.txt");
ofstream out("final1.txt");

const int DEFAULT_CACHE_SIZE=64;  // Default cache size
const int BLOCK_SIZE=4;            // Block size (4 bytes)
const int BLOCK_COUNT=DEFAULT_CACHE_SIZE/BLOCK_SIZE; // Number of blocks in cache
int Clock;
bool finito;

/*  Instr.   opCode
    label :  0000000
    rtype :  0110011
    imm   :  0010011
    lw    :  0000011
    st    :  0100011
    jump :  1101111
    branch:  1100011 

    operations supported:
    add
    sub
    xor
    or
    and

    funct3(for r type):
    add: 000
    sub: 000
    or:  110
    xor: 100
    and: 111

    funct3(for branch type):
    beq: 000 (branch if equal)
    bne: 001 (branch if not equal)
    blt: 100 (branch if less than)
    bge: 101 (branch if greater than or equal)

    funct7:
    add: 0000000
    sub: 0100000
    or:  0000000
    xor: 0000000
    and: 0000000

    note: load and store use direct memory access for simplicity instead of register relative
      */     


class Assembler{
  public:
    vector<string> vec;
    Assembler(){
        opCode.insert({"rtype", "0110011"});
        opCode.insert({"imm", "0010011"});
        opCode.insert({"lw", "0000011"});
        opCode.insert({"st", "0100011"});
        opCode.insert({"jal", "1101111"});
        opCode.insert({"branch", "1100011"});
                                                
        funct3.insert({"add", "000"});
        funct3.insert({"sub", "000"});
        funct3.insert({"or", "110"});
        funct3.insert({"xor", "100"});
        funct3.insert({"and", "111"});
   
        funct3.insert({"beq", "000"}); 
        funct3.insert({"bne", "001"}); 
        funct3.insert({"blt", "100"}); 
        funct3.insert({"bge", "101"}); 

        funct7.insert({"add", "0000000"});
        funct7.insert({"sub", "0100000"});
        funct7.insert({"or", "0000000"});
        funct7.insert({"xor", "0000000"});
        funct7.insert({"and", "0000000"});
    }

    string decToBinary(string reg){
        //takes input as: $3,$4,%10 etc. and converts it to output as a 5 bit string: "00011", "00100", "01010"
        if(reg[0]=='$' || reg[0]=='%') reg.erase(reg.begin());//erase the first symbol of the input register string
        int number=stoi(reg);//convert the string "3" to integer 3
        bitset<5> b(number);//get the binary representation of the integer 
        string temp=b.to_string();//convert the binary representation into a string
        return temp;
    }

    vector<string> elements(string inst){
        vector<string> el;//vector to store the different elements of a single line instruction input
        //e.x. addi $1,$2,10 ===> {"addi", "$1", "$2", "10"}
        string temp="";//temporary string to store the elements being added into the vector
        int ind=-1;
        
        for(int i=0;i<inst.size();i++){
            if(inst[i]==' '){//finding the first element that's present until the first whitespace
                ind=i+1;
                break;
            }
            temp=temp+inst[i];
        }
        
        if(ind==-1){//no whitespace in instruction implying that it's just a label
            inst.erase(inst.begin()+inst.size()-1);//erasing the colon from the label i.e. "loop:" --> "loop"
            el.push_back(inst);
            return el;
        }
        
        el.push_back(temp);
        temp="";//reset temporary string

        bool found=0;
        
        for(int i=ind;i<inst.size();i++){
            if(inst[i]==','){//finding next element upto the first comma
                ind=i+1;
                found=1;
                break;
            }
            temp=temp+inst[i];
        }

        found=0;
        el.push_back(temp);
        temp="";
        for(int i=ind;i<inst.size();i++){
            if(inst[i]==','){//finding next element from the first comma upto the second comma
                found=1;
                ind=i+1;
                break;
            }
            temp=temp+inst[i];
        }
        
        if(!found){//second comma not found, so just appending the last element and returning
            el.push_back(temp);
            temp="";
            return el;
        }

        el.push_back(temp);//pushing back the element between the two commas to the vector
        temp="";
        
        el.push_back(inst.substr(ind,inst.size()-ind));//pushing back the element after the second comma to the vector
        return el;
    }

    string instructionToMachine(string inst){
        //converts the elements i.e. {"addi", "$2", "$3", "10"} to a 32-bit machineCode string
        vector<string> el=elements(inst);//extract the individual elements of the instruction
        string temp;
        
        if(el.size()>1){//instruction is not just a label
            if (el[0] == "add" || el[0] == "sub" || el[0] == "xor" || el[0] == "and" || el[0] == "or")
            {
                //R-type instruction
                temp=temp+opCode["rtype"];//bits 0:6 -- opCode for R-type instruction
                temp=temp+decToBinary(el[1]);//bits 7:11 -- first register index binary value
                temp=temp+funct3[el[0]];//bits 12:14 -- funct3
                temp=temp+decToBinary(el[2]);//bits 15:19 -- second register index binary value
                temp=temp+decToBinary(el[3]);//bits 20:24 -- third register index binary value
                temp=temp+funct7[el[0]];//bits 25:31 -- funct7
            }
            else if (el[0] == "addi" || el[0] == "xori" || el[0] == "andi" || el[0] == "ori")
            {
                //imm type
                temp=temp+opCode["imm"];//bits 0:6 -- opCode for imm-type instruction
                temp=temp+decToBinary(el[1]);//bits 7:11 -- first register index binary value
                temp=temp+funct3[el[0].substr(0,3)];//bits 12:14 -- funct3
                temp=temp+decToBinary(el[2]);//bits 15:19 -- second register index binary value
                string se=el[3];//imm value
                int number=stoi(se);
                bitset<12> b(number);
                temp=temp+b.to_string();//bits 20:31 -- imm binary value
            }
            else if(el[0]=="jal"){//jump and link instruction //encoding simplified for the purposes of this project
                temp=temp+opCode["jal"];//bits 0:6 -- opCode for Jump instruction
                temp=temp+decToBinary(el[1]);//bits 7:11 -- first register index binary value
                int index=indicesOfLabels[el[2]];//getting the labelIndex of the label
                bitset<20> b(index);//converting the labelIndex to a 20-bit binary string
                temp=temp+b.to_string();//bits 12:31 -- labelIndex to jump to
            }
            else if (el[0] == "beq" || el[0] == "bne" || el[0] == "blt" || el[0] == "bge"){//branch instruction //encoding simplified for the purposes of this project
                temp=temp+opCode["branch"];//bits 0:6 -- opCode for Branch instruction
                int index=indicesOfLabels[el[3]];//getting the labelIndex of the label
                bitset<12> b(index);//converting the labelIndex to a 12-bit binary string
                string w=b.to_string();
                temp=temp+w.substr(w.size() - 5);//bits 7:11 -- last 5 bits of labelIndex to branch to
                temp=temp+funct3[el[0]]; //bits 12:14 --funct3 for branch 
                temp=temp+decToBinary(el[1]);//bits 15:19-- first register index binary value
                temp=temp+decToBinary(el[2]);//bits 20:24 -- second register index binary value
                temp=temp+w.substr(0,7);//bits 25:31 -- first 7 bits of labelIndex to branch to
            } 
            else if(el[0]=="lw"){//load instruction //encoding modified for direct memory access
                temp=temp+opCode[el[0]];//bits 0:6 -- opCode for Load instruction
                temp=temp+decToBinary(el[1]);//bits 7:11 -- first register index binary value
                string funct3_for_load="000"; //only lw implemented
                temp=temp+funct3_for_load; //bits 12:14 --funct3 for lw 
                string mem=el[2];//memory element of instruction e.x. mem="%2"
                mem.erase(mem.begin());//"%2" --> "2"
                int x=stoi(mem);//converting string to corresponding integer value
                bitset<17> b(x);//creating a 17 bit binary representation of the integer
                temp=temp+b.to_string();//bits 15:31 -- memory index which is to be accessed
            }
            else if(el[0]=="st"){//store instruction //encoding modified for direct memory access
                temp=temp+opCode[el[0]];//bits 0:6 -- opCode for Store instruction
                string mem=el[2];//memory element of instruction e.x. mem="%2"
                mem.erase(mem.begin());//"%2" --> "2"
                int x=stoi(mem);//converting string to corresponding integer value
                bitset<17> b(x);//creating a 17 bit binary representation of the integer
                string w=b.to_string();
                temp=temp+w.substr(w.size() - 5);//bits 7:11 -- last 5 bits of mem location
                string funct3_for_store="000"; //only sw implemented
                temp=temp+funct3_for_store; //bits 12:14 --funct3 for sw 
                temp=temp+w.substr(w.size() - 10, 5); //bits 15-19-- next 5 bits of mem location
                temp=temp+decToBinary(el[1]); //bits 20-24-- reg index
                temp=temp+w.substr(0, 7);//bits 25:31 -- first 7 bits of mem location
            }

        }
        else{//instruction is just a label
            if(el[0]=="*end"){
                for(int i=0;i<32;i++)temp=temp+'1';//we've set all bits to '1' to signify the end of the program
                return temp;
            }
            for(int i=0;i<27;i++)temp=temp+'0';//first 27 bits are '0' for a label instruction
            int index=indicesOfLabels[el[0]];//convert the line index of the first appearance of label to integer
            bitset<5> b(index);//converting the index of the label to binary
            temp=temp+b.to_string();//appending the binary string to the 27 '0' bits and returning the 32-bit instruction
        }
        return temp;
    }
    vector<int> whatIsRead(vector<string> el){
        vector<int> read;
        for(int i=0;i<el.size();i++){
            if(el[i][0]=='$'){
                el[i].erase(el[i].begin());
            }
        }
        string op;
        if (el[0] == "add" || el[0] == "sub" || el[0] == "xor" || el[0] == "and" || el[0] == "or") op="rtype";
        else if (el[0] == "addi" || el[0] == "xori" || el[0] == "andi" || el[0] == "ori") op="imm";
        else if(el[0]=="jal") op="jump";
        else if (el[0] == "beq" || el[0] == "bne" || el[0] == "blt" || el[0] == "bge") op="branch";
        else if(el[0]=="lw") op="lw";
        else if(el[0]=="st") op="st";
        if(op=="rtype"){
            read.push_back(stoi(el[2]));
            read.push_back(stoi(el[3]));
        }
        else if(op=="imm")
        {
            read.push_back(stoi(el[2]));
        }
        else if(op=="branch"){
            read.push_back(stoi(el[1]));
            read.push_back(stoi(el[2]));
        }
        else if(op=="st"){
            read.push_back(stoi(el[1]));
        }
        return read;
    }

    int whereToWrite(vector<string> el){
        for(int i=0;i<el.size();i++){
            if(el[i][0]=='$'){
                el[i].erase(el[i].begin());
            }
        }
        string op;
        if (el[0] == "add" || el[0] == "sub" || el[0] == "xor" || el[0] == "and" || el[0] == "or") return stoi(el[1]);
        if (el[0] == "addi" || el[0] == "xori" || el[0] == "andi" || el[0] == "ori") return stoi(el[1]);
        if(el[0]=="jal") return stoi(el[1]);
        if(el[0]=="lw") return stoi(el[1]);
        return -1;
    }

    vector<string> generateMachineCode(){
        //takes assembly code input and returns the corresponding vector 
        //containing the machine code counterparts of the assembly code
        vector<string> instructions;
        string end="*end:";
        string s; 
        
        while(inp){
            getline(inp,s);
            instructions.push_back(s);
            if(s==end)break;
        }

        //dealing with hazards by inserting waste instructions 
        for(int i=0;i<instructions.size();i++){
            vector<string> temp=elements(instructions[i]);
            int val=whereToWrite(temp);
            // if(temp[0]!="*end" && temp.size()==1){
            //     string vec="addi $31,$0,0";
            //     //to recognize labels which we will be passing in as instructions,it takes 3 clock cycles so insertion of 3 dummy instructions
            //     instructions.insert(instructions.begin()+i+1,vec);
            //     instructions.insert(instructions.begin()+i+1,vec);
            //     instructions.insert(instructions.begin()+i+1,vec);
            //     continue;
            // }
            vector<int> reading;
            int k=instructions.size();
            if(i<k-1){
                bool cond=0;
                reading=whatIsRead(elements(instructions[i+1]));
                for(auto x: reading){
                    if(x==val){
                        string vec="addi $31,$0,0";
                        instructions.insert(instructions.begin()+i+1,vec);
                        instructions.insert(instructions.begin()+i+1,vec);
                        instructions.insert(instructions.begin()+i+1,vec);
                        cond=1;
                        break;
                    }
                }
                if(cond)continue;
            }
            if(i<k-2){
                bool cond=0;
                reading=whatIsRead(elements(instructions[i+2]));
                for(auto x: reading){
                    if(x==val){
                        string vec="addi $31,$0,0";
                        instructions.insert(instructions.begin()+i+1,vec);
                        instructions.insert(instructions.begin()+i+1,vec);
                        cond=1;
                        break;
                    }
                }
                if(cond)continue;
            }
            if(i<k-3){
                bool cond=0;
                reading=whatIsRead(elements(instructions[i+3]));
                for(auto x: reading){
                    if(x==val){
                        string vec="addi $31,$0,0";
                        instructions.insert(instructions.begin()+i+1,vec);
                        cond=1;
                        break;
                    }
                }
                if(cond)continue;
            }
        }

        vec=instructions;
        //filling the appropriate line index for labels for jump/branch
        for(int i=0;i<instructions.size();i++){
            if(elements(instructions[i])[0]!="*end" && elements(instructions[i]).size()==1){//if instruction is just a label and it's not the *end label
                indicesOfLabels.insert({elements(instructions[i])[0],i});
            }
        }

        //converting the assembly code into machine code
        vector<string> machineCode;
        for(int i=0;i<instructions.size();i++){
            out<<instructions[i]<<endl;
            string temp=instructionToMachine(instructions[i]);
            machineCode.push_back(temp);
        }

        return machineCode;
    }

};

class Fetch{
	private:
		int PC=0;
		string Instruction;
        vector<string> MachineCode;
	public:
        Fetch(){}
        void setMachineCode(vector<string> x){
            MachineCode=x;
        }
		void setInstruction(vector<string> x){
            if(PC<=x.size()-1){
                if(BranchPossible==0){//in case of a branch, we don't fetch a new instruction until the zero flag is generated
                    Instruction=MachineCode[PC];
                    PC++;
                }
            } 
		}
		int getPC(){
			return PC;
		}	 

		void setPC(int k){
			PC=k;
		}

		string getOpcode(){
			return Instruction.substr(0,7);
		}

        string getIn(int m, int n){
            return Instruction.substr(m, n);
        }

        void giveToRegister(){
            //transferring the necessary information to the IF/ID global registers 
            if(EndFlag==0){
                if(BranchPossible==0){
                    if(JumpFlag==1){
                        //processing required for jump instruction
                        setPC(JA);
                        Instruction=MachineCode[PC];
                        PC++;
                        JumpFlag=0;
                    }
                    //storing the opCode and Instruction in the IF/ID registers
                    R_getOpCode=Instruction.substr(0,7);
                    R_Instruction=Instruction;
                }
                if(BranchPossible==1 && zeroFlagGenerated==1){
                    if(ZeroFlag==1){
                        //processing for a taken branch 
                        setPC(BA);
                        Instruction=MachineCode[PC];
                        PC++;
                        ZeroFlag=0;
                        BA=0;
                    }
                    R_getOpCode=Instruction.substr(0,7);
                    R_Instruction=Instruction;
                    zeroFlagGenerated=0;
                    BranchPossible=0;
                }
                else if(BranchPossible){
                    //zero flag not generated yet
                    R_getOpCode=Instruction.substr(0,7);
                    R_Instruction=Instruction;
                    R_PC=PC;
                }
                BranchPossible=0;
                ZeroFlag=0;
            }
        }
};


class ControlUnit
{
    private:
        string getOpCode, Instruction;
        bool DoSomething;

    public:
        ControlUnit(){
            DoSomething=0;
        }

        void takeFromRegister(){
            //takes the necessary data from the IF/ID registers
            if(!R_getOpCode.empty()){
                DoSomething=1;
                getOpCode=R_getOpCode;
                Instruction=R_Instruction;
                R_Instruction.clear();
                R_getOpCode.clear();
                //once taken, clearing the data of the IF/ID global registers
                if(Instruction.substr(0,7)==opCode["jal"]){//jump instruction
                    JumpFlag=1;
                    JA=stoi(Instruction.substr(12,20),0,2);
                } 
                if(Instruction.substr(0,7)==opCode["branch"]){//branch instruction
                    BranchPossible=1;
                    string ba=Instruction.substr(25,7);
                    ba+=Instruction.substr(7,5);
                    BA=stoi(ba,0,2);
                }
                if(Instruction.substr(0,7)=="111111") EndFlag=1; //*end:
            }
        }
        //rtype imm jal load will have rd
	    int RegDst(){
			if(getOpCode==opCode["rtype"] || getOpCode==opCode["imm"] || getOpCode==opCode["jal"] || getOpCode==opCode["ld"]) return stoi(Instruction.substr(7,5), 0, 2);//returns index of register to be written
            else return -1;
		}
		//checks if branch instruction
		bool Branch(){
			if(getOpCode==opCode["branch"]) return 1;
			else return 0;
		}
		//load instruction
		bool MemRead(){
		   	if(getOpCode==opCode["lw"]) return 1;
			else return 0;
		}
		//load
		bool MemtoReg(){
			if(getOpCode==opCode["lw"]) return 1;
			else return 0;
		}
		//store
		bool MemWrite(){
			if(getOpCode==opCode["st"]) return 1;
			else return 0;
		}

        bool Jump(){
            if(getOpCode==opCode["jal"])return 1;
            else return 0;
        }
        int ALUInput1(){
			if(getOpCode==opCode["lw"]|| getOpCode==opCode["st"] ||  getOpCode==opCode["jal"]) return 0;//load/store
            else return R[stoi(Instruction.substr(15, 5), 0, 2)];//second register of instruction orderwise
        }

		int ALUInput2(){
			if(getOpCode==opCode["rtype"]){
                 return R[stoi(Instruction.substr(20, 5), 0, 2)];//register value for R-type, orderwise 3rd register
            }
            else if(getOpCode==opCode["imm"])
            {
                return stoi(Instruction.substr(20, 12), 0, 2);//immediate R-type
            }
            else if(getOpCode==opCode["branch"]) return R[stoi(Instruction.substr(20,5), 0, 2)];//branch, first register orderwise
            else if(getOpCode==opCode["st"])
            {
                string address=Instruction.substr(25, 7);
                address=address+Instruction.substr(15, 5);
                address=address+Instruction.substr(7, 5);
                return stoi(address, 0, 2);
            }
            else if(getOpCode==opCode["jal"]) return R_PC;
            return stoi(Instruction.substr(15, 17), 0, 2);
		}
		
		bool RegWrite(){
			if(getOpCode==opCode["reg"] || getOpCode==opCode["imm"] ||  getOpCode==opCode["lw"] || getOpCode==opCode["jal"]) return 1;
			else return 0;
		}

        int dataForStore(){//which data to store for st
            return R[stoi(Instruction.substr(20, 5), 0, 2)];
        }
		
		int ALUControl(){
            //tells the ALU which operation to perform
            /*0: add
              1: sub
              2: or
              3:xor
              4:and*/
            if(getOpCode==opCode["branch"]) return 1;//branch
            else if (getOpCode==opCode["jal"]) return 0;
            else if(getOpCode==opCode["st"] || getOpCode==opCode["lw"]) return 0;//load/store
            else{
                if(Instruction.substr(12,3)==funct3["add"])//funct3 is for add/sub
                {
                    if(getOpCode==opCode["imm"]) return 0;
                    if(Instruction.substr(25,7)==funct7["add"]) return 0;//add
                    return 1;//sub
                }
                else if(Instruction.substr(12,3)==funct3["or"]) return 2;//funct3 is for or
                else if(Instruction.substr(12,3)==funct3["xor"]) return 3;//xor
                else return 4;//funct3 is and
            }
        }

		bool ALUDo(){
			if(getOpCode=="0000000" || getOpCode=="1111111") return 0;//label or end
			else return 1;
		}
		
		bool MemDo(){
			if(getOpCode==opCode["lw"] || getOpCode==opCode["st"]) return 1;//for load/store, memory unit will be active
			else return 0;
		}

		bool WritebackDo(){
			if(getOpCode==opCode["rtype"] || getOpCode==opCode["imm"] || getOpCode==opCode["jal"]) return 1;//for R-type/load, writeBack unit will be active
			else return 0;
		}

        bool End(){
            if(getOpCode=="1111111") return 1;
            return 0;
        }

        int branch_type()
        {
            string branch_funct3=Instruction.substr(12,3);
            if(branch_funct3==funct3["beq"]) return 0;
            if(branch_funct3==funct3["bne"]) return 1;
            if(branch_funct3==funct3["blt"]) return 2;
            return 3; //bge
        }

        void giveToRegister(){
            if(DoSomething==1){
                //giving the ID/EX registers the required data
                R_RegDst=RegDst();
                R_Branch=Branch();
                // R_BranchAddress=BranchAddress();
                // R_BranchAddress=BA;
                R_MemRead=MemRead();
                R_MemtoReg=MemtoReg();
                R_MemWrite=MemWrite();
                R_Jump=Jump();
                // R_JumpAddress=JumpAddress();
                R_ALUInput1=ALUInput1();
                R_ALUInput2=ALUInput2();
                R_RegWrite=RegWrite();
                R_dataForStore=dataForStore();
                R_ALUControl=ALUControl();
                R_ALUDo=ALUDo();
                R_MemDo=MemDo();
                R_WritebackDo=WritebackDo();
                R_End=End();
                DoSomething=0;
                R_btype=branch_type();
            }
            
        }
};

class ALU{
  private:
    int ALUInput1;
    int ALUInput2;
    bool ALUdo;
    bool ALUBranch;
    bool ALUMemRead;
    bool ALUMemToReg; 
    bool ALUMemWrite;
    bool ALURegWrite;
    bool ALUMemDo;
    bool ALUWriteBackDo;
    int passDataForSt;
    int regAddress;
    int ALUControl;
    int ALUbtype;
    bool end;
  public:
    void takeFromRegister(){
        //take the necessary data from the ID/EX registers
        ALUdo = R_ALUDo;
        ALUInput1 = R_ALUInput1;
        ALUInput2 = R_ALUInput2;
        ALUBranch = R_Branch;
        ALUMemRead = R_MemRead;
        ALUWriteBackDo = R_WritebackDo;
        ALUMemDo = R_MemDo;
        ALURegWrite = R_RegWrite;
        ALUMemWrite = R_MemWrite;
        ALUMemToReg = R_MemtoReg;
        ALUMemRead = R_MemRead;
        passDataForSt = R_dataForStore;
        regAddress = R_RegDst;
        ALUControl=R_ALUControl;
        ALUbtype=R_btype;
        end=R_End;
        if(ALUdo){
            if(ALUInput1-ALUInput2==0 && ALUBranch && ALUbtype==0){
                ZeroFlag = true;
            }
            if(ALUInput1-ALUInput2!=0 && ALUBranch && ALUbtype==1){
                ZeroFlag = true;
            }
            if(ALUInput1-ALUInput2<0 && ALUBranch && ALUbtype==2){
                ZeroFlag = true;
            }
            if(ALUInput1-ALUInput2>=0 && ALUBranch && ALUbtype==3){
                ZeroFlag = true;
            }
            if(ALUBranch){
                zeroFlagGenerated = true;
            }
            R_ALUDo = false;
        }
}

    void giveToRegister(){
        if(ALUControl==0 && ALUdo){ 
            R_ALUOutputReg =  ALUInput1+ALUInput2;//funct add
        }

        else if(ALUControl==1 && ALUdo){
            R_ALUOutputReg = ALUInput1-ALUInput2;//funct subtract
        }

        else if(ALUControl==2 && ALUdo){
            R_ALUOutputReg = ALUInput1|ALUInput2;//funct or
        }
        else if(ALUControl==3 && ALUdo){
            R_ALUOutputReg = ALUInput1^ALUInput2;//funct xor
        }
        else if(ALUControl==4 && ALUdo){
            R_ALUOutputReg = ALUInput1&ALUInput2;//funct and
        }
        
        R_memRead1 = ALUMemRead; 
        R_writeBackDo1 = ALUWriteBackDo;
        R_memDo1 = ALUMemDo;
        R_regWrite1 = ALURegWrite;
        R_memWrite1 = ALUMemWrite;
        R_memToReg1 = ALUMemToReg;
        R_memWriteData1 = passDataForSt;
        R_regDst1 = regAddress;
        R_end1 = end;
    }
};

struct CacheLine {
    int tag;
    int data[BLOCK_SIZE];  // Array to hold all data for the block
    bool valid;

    CacheLine() : tag(-1), valid(false) {
       fill_n(data, BLOCK_SIZE, 0);  // Initialize data to zero
    }
};

class Cache {
private:
    vector<CacheLine> cache;

public:
    Cache() {
        cache.resize(BLOCK_COUNT);
    }

    void getTagIndexOffset(int address, int &tag, int &index, int &offset) {
        int offsetBits = log2(BLOCK_SIZE);               // 2 bits for offset
        int indexBits = log2(BLOCK_COUNT);               // 4 bits for index

        // Calculate offset
        offset = address & (BLOCK_SIZE - 1);             // Last 2 bits for offset

        // Calculate index
        index = (address / BLOCK_SIZE) % BLOCK_COUNT;    // Index into cache

        // Calculate tag
        tag = address >> (offsetBits + indexBits);       // Higher bits for tag
    }

    int read(int address) {
        int tag, index, offset;
        getTagIndexOffset(address, tag, index, offset);

        if (cache[index].valid && cache[index].tag == tag) {
            return cache[index].data[offset]; // Return the specific byte in the block
        } else {
            // Load block from main memory into cache
            cache[index].tag = tag;
            for (int i = 0; i < BLOCK_SIZE; ++i) {
                cache[index].data[i] = MEM[(address / BLOCK_SIZE) * BLOCK_SIZE + i]; // Load entire block
            }
            cache[index].valid = true;
            return cache[index].data[offset]; // Return the specific byte in the block
        }
    }

    void write(int address, int data) {
        int tag, index, offset;
        getTagIndexOffset(address, tag, index, offset);

        // Load block if it's not valid
        if (!cache[index].valid || cache[index].tag != tag) {
            cache[index].tag = tag;
            for (int i = 0; i < BLOCK_SIZE; ++i) {
                cache[index].data[i] = MEM[(address / BLOCK_SIZE) * BLOCK_SIZE + i]; // Load entire block
            }
            cache[index].valid = true;
        }

        // Write data to the correct offset in the block
        cache[index].data[offset] = data;
        MEM[address] = data; // Also write back to main memory
    }
};

class Memory{
  private:
    bool memRead;
    bool memWrite;
    bool memDo;
    bool regWrite; 
    bool writeBackDo;
    int ALUOutput;
    int dataToBeStored;
    int readData;
    int regDst;
    bool end;
    Cache& cache;  // Reference to the cache
  public:
    Memory(Cache& cacheRef) : cache(cacheRef) {}
    void takeFromRegister(){ 
        //takes the necessary data from EX/MEM registers
        memWrite = R_memWrite1;
        memDo = R_memDo1;
        ALUOutput = R_ALUOutputReg;
        dataToBeStored = R_memWriteData1;
        memRead = R_memRead1;
        writeBackDo = R_writeBackDo1;
        regWrite = R_regWrite1;
        regDst = R_regDst1;
        end=R_end1;
        if(memWrite==1 && memDo==1) { 
            cache.write(ALUOutput, dataToBeStored);
            R_MemDo=false;  
        }
    }

    void giveToRegister(){
        if(memRead==1 && memDo==1){
            readData=cache.read(ALUOutput);
        }
        else if(memRead==0 && memDo==0){//by-pass
            readData = ALUOutput;
        }
        R_writeBackData = readData;//data to write
        R_writeBackDo2 = writeBackDo;//wb on/off state
        R_regDst2 = regDst;//where to write
        R_end2=end;
    }
};

class Writeback{
  private:
    int dataToBeWritten;
    int regDst;
    bool writeDo;
    bool end;
  public:
    void takeFromRegister(){
        //taking the necessary data from MEM/WB registers
        writeDo = R_writeBackDo2;
        regDst = R_regDst2;
        if(writeDo){
            dataToBeWritten = R_writeBackData;
            R_WritebackDo = false;
        }
        end=R_end2;
    }

    void writeBackToRegister(){
        if(writeDo==1){
            R[regDst] = dataToBeWritten;
        }
        finito=end;
    }
};

bool stall(Fetch* f){
    if(f->getIn(0,7)==opCode["branch"]) return 1;
    return 0;
}

void print(){
    out<<"\nValue of registers: ";
    for(int i=0;i<10;i++){
        out<<R[i]<<" ";
    }
    out<<"\n";
    out<<"\nValue in memory: ";
    for(int i=0;i<10;i++){
        out<<MEM[i]<<" ";
    }
    out<<"\n\n";
    for(int i=0;i<69;i++)out<<"-";
    out<<"\n";
}

int main(){
    vector<string> v;
    Assembler* assembler = new Assembler();
    Fetch* f=new Fetch();
    v=assembler->generateMachineCode();
    f->setMachineCode(v);
    ControlUnit* c=new ControlUnit();
    ALU* alu=new ALU();
    Cache* cache=new Cache();
    Memory* mem=new Memory(*cache);
    Writeback* wb=new Writeback();

    int cnt=0, meaningfulInstructions=0;
    bool skip=0;
    while(!finito){
        if(!skip){
            f->setInstruction(v);
            if(f->getIn(0,32) != "00100111111100000000000000000000") meaningfulInstructions++; //if not addi $31,$0,0 which is our dummy instruction
        }

        c->takeFromRegister();
        alu->takeFromRegister();
        mem->takeFromRegister();
        wb->takeFromRegister();
        if(skip){
            skip=0;
            cnt++;
        }

        f->giveToRegister();
        c->giveToRegister();
        alu->giveToRegister();
        mem->giveToRegister();
        wb->writeBackToRegister();

        if(cnt<1)skip=stall(f);
        else cnt=0;
        
        Clock++;
        print();
    }
    out<<"Clocks: "<<Clock<<"\n";
    out<<"CPI : "<<setprecision(10)<<float(float(Clock)/meaningfulInstructions)<<"\n";
    return 0;
}