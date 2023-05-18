#include <unordered_map>
#include <string>
#include <functional>
#include <vector>
#include <fstream>
#include <exception>
#include <iostream>
#include <boost/tokenizer.hpp>

using namespace std;

struct IF_ID_inter {
	string reg1;
	string reg2;
	string reg3;
	string opcode;
};

struct ID_EX_inter {
	string reg1;
	string reg2;
	string reg3;
	string opcode;
	int result;
	int r1_val;
	int r2_val;
	int r3_val;
};

struct EX_MEM_inter {
	string reg1;
	string reg2;
	int result;
	int data;
	string opcode;
};

struct MEM_WB_inter {
	string reg1;
	int data;
	int result;
	string opcode;
};

struct MIPS_Architecture
{
	int registers[32] = {0}, PCcurr = 0, PCnext;
	unordered_map<string, function<int(MIPS_Architecture &, string, string, string)>> instructions;
	unordered_map<string, int> registerMap, address;
	static const int MAX = (1 << 20);
	int data[MAX >> 2] = {0};
	unordered_map<int, int> memoryDelta;
	vector<vector<string>> commands;
	vector<int> commandCount;
	enum exit_code
	{
		SUCCESS = 0,
		INVALID_REGISTER,
		INVALID_LABEL,
		INVALID_ADDRESS,
		SYNTAX_ERROR,
		MEMORY_ERROR
	};

	IF_ID_inter IF_ID;
	ID_EX_inter ID_EX;
	EX_MEM_inter EX_MEM;
	MEM_WB_inter MEM_WB;

	bool stall = false;

	int clockCycles;
	int exitcode;

	bool valid_if = false;
	bool valid_id = false;
	bool valid_ex = false;
	bool valid_mem = false;
	bool valid_wb = false;

	unordered_map<string, int> occupied;

	// constructor to initialise the instruction set
	MIPS_Architecture(ifstream &file)
	{
		instructions = {{"add", &MIPS_Architecture::add}, {"sub", &MIPS_Architecture::sub}, {"mul", &MIPS_Architecture::mul}, {"beq", &MIPS_Architecture::beq}, {"bne", &MIPS_Architecture::bne}, {"slt", &MIPS_Architecture::slt}, {"j", &MIPS_Architecture::j}, {"lw", &MIPS_Architecture::lw}, {"sw", &MIPS_Architecture::sw}, {"addi", &MIPS_Architecture::addi}};

		for (int i = 0; i < 32; ++i)
			registerMap["$" + to_string(i)] = i;
		registerMap["$zero"] = 0;
		registerMap["$at"] = 1;
		registerMap["$v0"] = 2;
		registerMap["$v1"] = 3;
		for (int i = 0; i < 4; ++i)
			registerMap["$a" + to_string(i)] = i + 4;
		for (int i = 0; i < 8; ++i)
			registerMap["$t" + to_string(i)] = i + 8, registerMap["$s" + to_string(i)] = i + 16;
		registerMap["$t8"] = 24;
		registerMap["$t9"] = 25;
		registerMap["$k0"] = 26;
		registerMap["$k1"] = 27;
		registerMap["$gp"] = 28;
		registerMap["$sp"] = 29;
		registerMap["$s8"] = 30;
		registerMap["$ra"] = 31;

		constructCommands(file);
		commandCount.assign(commands.size(), 0);
	}

    int add(std::string r1, std::string r2, std::string r3)
	{
		return op(r1, r2, r3, [&](int a, int b)
				  { return a + b; });
	}

	// perform subtraction operation
	int sub(std::string r1, std::string r2, std::string r3)
	{
		return op(r1, r2, r3, [&](int a, int b)
				  { return a - b; });
	}

	// perform multiplication operation
	int mul(std::string r1, std::string r2, std::string r3)
	{
		return op(r1, r2, r3, [&](int a, int b)
				  { return a * b; });
	}

	// perform the binary operation
	int op(std::string r1, std::string r2, std::string r3, std::function<int(int, int)> operation)
	{
		if (!checkRegisters({r1, r2, r3}) || registerMap[r1] == 0)
			return 1;
		registers[registerMap[r1]] = operation(registers[registerMap[r2]], registers[registerMap[r3]]);
		PCnext = PCcurr + 1;
		return 0;
	}

	// perform the beq operation
	int beq(std::string r1, std::string r2, std::string label)
	{
		return bOP(r1, r2, label, [](int a, int b)
				   { return a == b; });
	}

	// perform the bne operation
	int bne(std::string r1, std::string r2, std::string label)
	{
		return bOP(r1, r2, label, [](int a, int b)
				   { return a != b; });
	}

	// implements beq and bne by taking the comparator
	int bOP(std::string r1, std::string r2, std::string label, std::function<bool(int, int)> comp)
	{
		if (!checkLabel(label))
			return 4;
		if (address.find(label) == address.end() || address[label] == -1)
			return 2;
		if (!checkRegisters({r1, r2}))
			return 1;
		PCnext = comp(registers[registerMap[r1]], registers[registerMap[r2]]) ? address[label] : PCcurr + 1;
		return 0;
	}

	// implements slt operation
	int slt(std::string r1, std::string r2, std::string r3)
	{
		if (!checkRegisters({r1, r2, r3}) || registerMap[r1] == 0)
			return 1;
		registers[registerMap[r1]] = registers[registerMap[r2]] < registers[registerMap[r3]];
		PCnext = PCcurr + 1;
		return 0;
	}

	// perform the jump operation
	int j(std::string label, std::string unused1 = "", std::string unused2 = "")
	{
		if (!checkLabel(label))
			return 4;
		if (address.find(label) == address.end() || address[label] == -1)
			return 2;
		PCnext = address[label];
		return 0;
	}

	// perform load word operation
	int lw(std::string r, std::string location, std::string unused1 = "")
	{
		if (!checkRegister(r) || registerMap[r] == 0)
			return 1;
		int address = locateAddress(location);
		if (address < 0)
			return abs(address);
		registers[registerMap[r]] = data[address];
		PCnext = PCcurr + 1;
		return 0;
	}

	// perform store word operation
	int sw(std::string r, std::string location, std::string unused1 = "")
	{
		if (!checkRegister(r))
			return 1;
		int address = locateAddress(location);
		if (address < 0)
			return abs(address);
		if (data[address] != registers[registerMap[r]])
			memoryDelta[address] = registers[registerMap[r]];
		data[address] = registers[registerMap[r]];
		PCnext = PCcurr + 1;
		return 0;
	}

    int addi(std::string r1, std::string r2, std::string num)
	{
		if (!checkRegisters({r1, r2}) || registerMap[r1] == 0)
			return 1;
		try
		{
			registers[registerMap[r1]] = registers[registerMap[r2]] + stoi(num);
			PCnext = PCcurr + 1;
			return 0;
		}
		catch (std::exception &e)
		{
			return 4;
		}
	}

	int locateAddress(string location)
	{
		if (location.back() == ')')
		{
			try
			{
				int lparen = location.find('('), offset = stoi(lparen == 0 ? "0" : location.substr(0, lparen));
				string reg = location.substr(lparen + 1);
				reg.pop_back();
				if (!checkRegister(reg))
					return -3;
				int address = registers[registerMap[reg]] + offset;
				if (address % 4 || address < int(4 * commands.size()) || address >= MAX)
					return -3;
				return address / 4;
			}
			catch (exception &e)
			{
				return -4;
			}
		}
		try
		{
			int address = stoi(location);
			if (address % 4 || address < int(4 * commands.size()) || address >= MAX)
				return -3;
			return address / 4;
		}
		catch (exception &e)
		{
			return -4;
		}
	}

	// checks if label is valid
	inline bool checkLabel(string str)
	{
		return str.size() > 0 && isalpha(str[0]) && all_of(++str.begin(), str.end(), [](char c)
														   { return (bool)isalnum(c); }) &&
			   instructions.find(str) == instructions.end();
	}

	// checks if the register is a valid one
	inline bool checkRegister(string r)
	{
		return registerMap.find(r) != registerMap.end();
	}

	// checks if all of the registers are valid or not
	bool checkRegisters(vector<string> regs)
	{
		return all_of(regs.begin(), regs.end(), [&](string r)
						   { return checkRegister(r); });
	}

	/*
		handle all exit codes:
		0: correct execution
		1: register provided is incorrect
		2: invalid label
		3: unaligned or invalid address
		4: syntax error
		5: commands exceed memory limit
	*/
	void handleExit(exit_code code, int cycleCount)
	{
		cout << '\n';
		switch (code)
		{
		case 1:
			cerr << "Invalid register provided or syntax error in providing register\n";
			break;
		case 2:
			cerr << "Label used not defined or defined too many times\n";
			break;
		case 3:
			cerr << "Unaligned or invalid memory address specified\n";
			break;
		case 4:
			cerr << "Syntax error encountered\n";
			break;
		case 5:
			cerr << "Memory limit exceeded\n";
			break;
		default:
			break;
		}
		if (code != 0)
		{
			cerr << "Error encountered at:\n";
			for (auto &s : commands[PCcurr])
				cerr << s << ' ';
			cerr << '\n';
		}
	}

	// parse the command assuming correctly formatted MIPS instruction (or label)
	void parseCommand(string line)
	{
		// strip until before the comment begins
		line = line.substr(0, line.find('#'));
		vector<string> command;
		boost::tokenizer<boost::char_separator<char>> tokens(line, boost::char_separator<char>(", \t"));
		for (auto &s : tokens)
			command.push_back(s);
		// empty line or a comment only line
		if (command.empty())
			return;
		else if (command.size() == 1)
		{
			string label = command[0].back() == ':' ? command[0].substr(0, command[0].size() - 1) : "?";
			if (address.find(label) == address.end())
				address[label] = commands.size();
			else
				address[label] = -1;
			command.clear();
		}
		else if (command[0].back() == ':')
		{
			string label = command[0].substr(0, command[0].size() - 1);
			if (address.find(label) == address.end())
				address[label] = commands.size();
			else
				address[label] = -1;
			command = vector<string>(command.begin() + 1, command.end());
		}
		else if (command[0].find(':') != string::npos)
		{
			int idx = command[0].find(':');
			string label = command[0].substr(0, idx);
			if (address.find(label) == address.end())
				address[label] = commands.size();
			else
				address[label] = -1;
			command[0] = command[0].substr(idx + 1);
		}
		else if (command[1][0] == ':')
		{
			if (address.find(command[0]) == address.end())
				address[command[0]] = commands.size();
			else
				address[command[0]] = -1;
			command[1] = command[1].substr(1);
			if (command[1] == "")
				command.erase(command.begin(), command.begin() + 2);
			else
				command.erase(command.begin(), command.begin() + 1);
		}
		if (command.empty())
			return;
		if (command.size() > 4)
			for (int i = 4; i < (int)command.size(); ++i)
				command[3] += " " + command[i];
		command.resize(4);
		commands.push_back(command);
	}

	// construct the commands vector from the input file
	void constructCommands(ifstream &file)
	{
		string line;
		while (getline(file, line))
			parseCommand(line);
		file.close();
	}

	// print the register data in hexadecimal
	void printRegistersAndMemoryDelta(int clockCycle)
	{
		for (int i = 0; i < 32; ++i)
			cout << registers[i] << ' ';
		cout << '\n';
		cout << memoryDelta.size();
		if(memoryDelta.empty()){
			if(!(valid_if && valid_id && valid_ex && valid_mem && valid_wb)){
				cout << '\n';
			}
		}
		else{
			cout << ' ';
		}
		for (auto &p : memoryDelta)
			cout << p.first << ' ' << p.second << '\n';
		memoryDelta.clear();
	}

	// Add instruction pipeline stages
	void IF_Stage() {
		// Fetch instruction from memory
		if(PCcurr < commands.size()){
			if(stall){
				return;
			}
			vector<string> &command = commands[PCcurr];
			if (instructions.find(command[0]) == instructions.end())
			{
				handleExit(SYNTAX_ERROR, clockCycles);
				return;
			}
			IF_ID.opcode = command[0];
			IF_ID.reg1 = command[1];
			IF_ID.reg2 = command[2];
			IF_ID.reg3 = command[3];
			// cout << "IF_Stage" << "\n";
			// cout << "opcode : " << command[0] << "\n";
			// cout << "r1 : " << command[1] << "\n";
			// cout << "r2 : " << command[2] << "\n";
			// cout << "r3 : " << command[3] << "\n";
		}
		else{
			valid_if = true;
			IF_ID.opcode = "done";
			return;
		}
	}

	void ID_Stage() {
		// Decode instruction
		if(IF_ID.opcode == ""){
			return;
		}
		if(IF_ID.opcode == "done"){
			valid_id = true;
			ID_EX.opcode = "done";
			return;
		}
		if(IF_ID.opcode == "j"){
			if(valid_if){
				valid_id = true;
			}
		}
		if(stall){
			return;
		}
		string opcode = IF_ID.opcode;
		string r1 = IF_ID.reg1;
		string r2 = IF_ID.reg2;
		string r3 = IF_ID.reg3;
		exitcode = 0;
		if(opcode == "add" || opcode == "sub" || opcode == "mul" || opcode == "slt"){
			if (!checkRegisters({r1, r2, r3}) || registerMap[r1] == 0) {
				exitcode = 1;
				return;
			}
			if(occupied.find(r2) != occupied.end() || occupied.find(r3) != occupied.end()){
				stall = true;
				ID_EX.opcode = "stalled";
				return;
			}
			ID_EX.r2_val = registers[registerMap[r2]];
			ID_EX.r3_val = registers[registerMap[r3]];
			occupied[r1]++;
			PCcurr++;
		}
		else if(opcode == "bne" || opcode == "beq"){
			if (!checkLabel(r3)){
				exitcode = 4;
				return;
			}
			if (address.find(r3) == address.end() || address[r3] == -1){
				exitcode = 2;
				return;
			}
			if (!checkRegisters({r1, r2})){
				exitcode = 1;
				return;
			}
			if(occupied.find(r1) != occupied.end() || occupied.find(r2) != occupied.end()){
				stall = true;
				ID_EX.opcode = "stalled";
				return;
			}
			ID_EX.r1_val = registers[registerMap[r1]];
			ID_EX.r2_val = registers[registerMap[r2]];
			if(opcode == "beq"){
				if(ID_EX.r1_val == ID_EX.r2_val){
					PCcurr = address[r3];
				}else{
					PCcurr++;
				}
			}
			else if(opcode == "bne"){
				if(ID_EX.r1_val != ID_EX.r2_val){
					PCcurr = address[r3];
				}else{
					PCcurr++;
				}
			}
		}
		else if(opcode == "addi"){
			if (!checkRegisters({r1, r2}) || registerMap[r1] == 0){
				exitcode = 1;
				return;
			}
			if(occupied.find(r2) != occupied.end()){
				stall = true;
				ID_EX.opcode = "stalled";
				return;
			}
			ID_EX.r1_val = registers[registerMap[r1]];
			ID_EX.r2_val = registers[registerMap[r2]];
			ID_EX.r3_val = stoi(r3);
			occupied[r1]++;
			PCcurr++;
		}
		else if(opcode == "j"){
			if (!checkLabel(r1)){
				exitcode = 4;
				return;
			}
			if (address.find(r1) == address.end() || address[r1] == -1){
				exitcode = 2;	
				return;
			}
			PCcurr = address[r1];
		}
		else if(opcode == "lw"){
			int lparen = r2.find('('), offset = stoi(lparen == 0 ? "0" : r2.substr(0, lparen));
			string reg = r2.substr(lparen + 1);
			reg.pop_back();
			if (!checkRegister(r1) || registerMap[r1] == 0){
				exitcode = 1;
				return;
			}
			if(occupied.find(r1) != occupied.end() || occupied.find(reg) != occupied.end()){
				stall = true;
				ID_EX.opcode = "stalled";
				return;
			}
			ID_EX.r1_val = registers[registerMap[r1]];
			ID_EX.r2_val = registers[registerMap[reg]];
			occupied[r1]++;
			PCcurr++;
		}
		else if(opcode == "sw"){
			int lparen = r2.find('('), offset = stoi(lparen == 0 ? "0" : r2.substr(0, lparen));
			string reg = r2.substr(lparen + 1);
			reg.pop_back();
			if (!checkRegister(r1)){
				exitcode = 1;	
				return;
			}
			if(occupied.find(r1) != occupied.end() || occupied.find(reg) != occupied.end()){
				stall = true;
				ID_EX.opcode = "stalled";
				return;
			}
			ID_EX.r1_val = registers[registerMap[r1]];
			ID_EX.r2_val = registers[registerMap[reg]];
			occupied[reg]++;
			PCcurr++;
		}
		ID_EX.reg1 = IF_ID.reg1;
		ID_EX.reg2 = IF_ID.reg2;
		ID_EX.reg3 = IF_ID.reg3;
		ID_EX.opcode = IF_ID.opcode;
		// printRegistersAndData(1);
	}

	void EX_Stage() {
		if(ID_EX.opcode == ""){
			return;
		}
		if(ID_EX.opcode == "done"){
			valid_ex = true;
			EX_MEM.opcode = "done";
			return;
		}
		if(ID_EX.opcode == "stalled"){
			EX_MEM.opcode = "stalled";
			return;
		}
		if(ID_EX.opcode == "beq" || ID_EX.opcode == "bne"){
			if(valid_if){
				valid_ex = true;
			}
		}
		// Execute instruction
		string opcode = ID_EX.opcode;
		string r1 = ID_EX.reg1;
		string r2 = ID_EX.reg2;
		string r3 = ID_EX.reg3;
		int r1_val = ID_EX.r1_val;
		int r2_val = ID_EX.r2_val;
		int r3_val = ID_EX.r3_val;
		EX_MEM.opcode = ID_EX.opcode;
		EX_MEM.reg1 = ID_EX.reg1;
		EX_MEM.reg2 = ID_EX.reg2;
		int result;

		if(opcode == "add"){
			result = r2_val + r3_val;
			// PCnext = PCcurr + 1;
		}
		else if(opcode == "sub"){
			result = r2_val - r3_val;
			// PCnext = PCcurr + 1;
		}
		else if(opcode == "mul"){
			result = r2_val*r3_val;
			// PCnext = PCcurr + 1;
		}
		else if(opcode == "addi"){
			result = r2_val + r3_val;
			// PCnext = PCcurr + 1;
		}
		else if(opcode == "beq"){
			result = r1_val == r2_val ? 1 : 0;
			// if(result == 1){
			// 	PCcurr = address[r3];
			// }else{
			// 	PCnext++;
			// }
		}
		else if(opcode == "bne"){
			result = r1_val != r2_val ? 1 : 0;
			// if(result == 1){
			// 	PCcurr = address[r3];
			// }else{
			// 	PCnext++;
			// }
		}
		else if(opcode == "slt"){
			result = r2_val < r3_val ? 1 : 0;
			// PCnext = PCcurr + 1;
		}
		// else if(opcode == "j"){
		// 	PCnext = address[r1];
		// }
		// else if(opcode == "lw" || opcode == "sw"){
		// 	PCnext = PCcurr + 1;
		// }
		EX_MEM.result = result;
		// printRegistersAndData(2);
	}

	void MEM_Stage() {
		if(EX_MEM.opcode == ""){
			return;
		}
		if(EX_MEM.opcode == "done"){
			valid_mem = true;
			MEM_WB.opcode = "done";
			return;
		}
		if(EX_MEM.opcode == "stalled"){
			MEM_WB.opcode = "stalled";
			return;
		}
		
		if(EX_MEM.opcode == "sw"){
			if(valid_if){
				valid_mem = true;
			}
		}
		
		
		// Access memory
		MEM_WB.reg1 = EX_MEM.reg1;
		MEM_WB.result = EX_MEM.result;
		MEM_WB.opcode = EX_MEM.opcode;
		
		if(MEM_WB.opcode == "lw"){
			int address = locateAddress(EX_MEM.reg2);
			MEM_WB.data = data[address];
		}
		else if(MEM_WB.opcode == "sw"){
			int address = locateAddress(EX_MEM.reg2);
			if (data[address] != registers[registerMap[EX_MEM.reg1]])
				memoryDelta[address] = registers[registerMap[EX_MEM.reg1]];
			data[address] = registers[registerMap[EX_MEM.reg1]];
			int lparen = EX_MEM.reg2.find('('), offset = stoi(lparen == 0 ? "0" : EX_MEM.reg2.substr(0, lparen));
			string reg = EX_MEM.reg2.substr(lparen + 1);
			reg.pop_back();
			occupied.erase(reg);
			stall = false;
			// PCnext = PCcurr + 1;	
		}	
		// printRegistersAndData(3);
	}

	void WB_Stage() {
		if(MEM_WB.opcode == ""){
			return;
		}
		if(MEM_WB.opcode == "done"){
			valid_wb = true;
			return;
		}
		if(MEM_WB.opcode == "stalled"){
			return;
		}
		if(MEM_WB.opcode == "lw" || MEM_WB.opcode == "add" || MEM_WB.opcode == "sub" || MEM_WB.opcode == "mul" || MEM_WB.opcode == "slt" || MEM_WB.opcode == "addi"){
			if(valid_if){
				valid_wb = true;
			}
		}
		// Write back result to register file		

		if(MEM_WB.opcode == "lw"){
			registers[registerMap[MEM_WB.reg1]] = MEM_WB.data;
			// PCnext = PCcurr + 1;
			occupied.erase(MEM_WB.reg1);
			if(occupied.empty()){
				stall = false;
			}
		}
		else if(MEM_WB.opcode == "add" || MEM_WB.opcode == "sub" || MEM_WB.opcode == "mul" || MEM_WB.opcode == "slt" || MEM_WB.opcode == "addi"){
			registers[registerMap[MEM_WB.reg1]] = MEM_WB.result;
			occupied.erase(MEM_WB.reg1);
			if(occupied.empty()){	
				stall = false;
			}
		}
		// printRegistersAndData(4);
	}

	void executeCommandsPipelined()
	{
		if (commands.size() >= MAX / 4)
		{
			handleExit(MEMORY_ERROR, 0);
			return;
		}

		clockCycles = 0;

		while (!(valid_if && valid_id && valid_ex && valid_mem && valid_wb))
		{
			++clockCycles;
			exitcode = 0;
			// cout << clockCycles << "\n";
			WB_Stage();
			// cout << "WB_Stage done" << "\n";
			MEM_Stage();
			// cout << "MEM_Stage done" << "\n";
			EX_Stage();
			// cout << "EX_Stage done" << "\n";
			ID_Stage();
			// cout << "ID_Stage done" << "\n";
			if(clockCycles > 1){
				IF_Stage();
			}
			// cout << "IF_Stage done" << "\n";
			// PCcurr = PCnext;
			printRegistersAndMemoryDelta(clockCycles);
		}
		handleExit(SUCCESS, clockCycles);
	}

	void printRegistersAndData(int stage)
    {   
        if (stage==1)
        {
            cout << "\tID STAGE" << endl;
            cout << "\t\topcode: " << IF_ID.opcode << endl;
            cout << "\t\tr1: " << IF_ID.reg1 << endl;
            cout << "\t\tr2: " << IF_ID.reg2 << endl;
            cout << "\t\tr3: " << IF_ID.reg3 << endl;
            cout << endl;

            cout << "\t\tr1_value: " << registers[registerMap[IF_ID.reg1]]<< endl;
            cout << "\t\tr2_value: " << registers[registerMap[IF_ID.reg2]] << endl;
            cout << "\t\tr3_value: " << registers[registerMap[IF_ID.reg3]] << endl;
        }   
        else if (stage==2)
        {
            cout << "\tEX STAGE" << endl;
            cout << "\t\topcode: " << ID_EX.opcode << endl;
            cout << "\t\tr1_value: " << ID_EX.r1_val << endl;
            cout << "\t\tr2_value: " << ID_EX.r2_val << endl;
            cout << "\t\tr3_value: " << ID_EX.r3_val << endl;
            cout << endl;
        }
        else if (stage==3)
        {
            cout << "\tMEM STAGE" << endl;
            cout << "\t\topcode: " << EX_MEM.opcode << endl;
            cout << "\t\tr1: " << EX_MEM.reg1 << endl;
            cout << "\t\tr2: " << EX_MEM.reg2 << endl;
            cout << "\t\tresult: " << EX_MEM.result << endl;
            cout << endl;
        }
        else if (stage==4)
        {
            cout << "\tWB STAGE" << endl;
            cout << "\t\topcode: " << MEM_WB.opcode << endl;
            cout << "\t\tr1: " << MEM_WB.reg1 << endl;
            cout << "\t\tresult: " << MEM_WB.result << endl;
            cout << "\t\tdata: " << MEM_WB.data << endl;
            cout << endl;
		}
	}

};

int main(int argc, char *argv[])
{
	if (argc != 2)
	{
		std::cerr << "Required argument: file_name\n./MIPS_interpreter <file name>\n";
		return 0;
	}
	std::ifstream file(argv[1]);
	MIPS_Architecture *mips;
	if (file.is_open())
		mips = new MIPS_Architecture(file);
	else
	{
		std::cerr << "File could not be opened. Terminating...\n";
		return 0;
	}

	mips->executeCommandsPipelined();
	return 0;
}
