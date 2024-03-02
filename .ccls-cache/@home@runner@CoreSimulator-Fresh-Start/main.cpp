/*

Adam Nelson-Archer (2140122)
COSC 3336
Programming Assignment 1

to compile this code:
use "g++ main.cpp -o [name]"
then "./[name] < input##.txt"

*/
// enable this to get detailed output logs like the output files:
bool verboseLog = false;
//

#include <iostream>
#include <queue>
#include <vector>
#include <functional>
#include <map>
#include <fstream>
#include <sstream>
#include <string>
using namespace std;

struct Process {
    int PID; // Process ID number
    float TimeCompletion; // Time completion or arrival time
    string Instruction; // Type of instruction
    int NumLogReads = 0; // Number of logical reads
    int NumPhyWrites = 0; // Number of physical writes
    int NumPhyReads = 0; // Number of physical reads
    int BufferSize = 0; // Buffer size, initially 0
};
struct Input {
    string command;
    float time;
};
struct ProcessEntry {
    int PID;
    int StartLine = 0; // Also starting at 0
    int EndLine = -1; // Initialized to -1 to adjust for 0-based indexing
    int CurrentLine = -1;
    int State = 0; // 0 = null, 1 = running, 2 = ready, 3 = blocked
};

// Comparator for the priority queue (min heap based on TimeCompletion)
struct ProcessCompare {
    bool operator()(const Process& a, const Process& b) {
        return a.TimeCompletion > b.TimeCompletion;
    }
};

// global variables & queues
int BSIZE = 0;
priority_queue<Process, vector<Process>, ProcessCompare> MainQueue; // Priority Queue of Processes
queue<Process> SQ; // SSD 
queue<Process> RQ; // CPU Queue
bool CpuIsEmpty = true;
bool SSDisEmpty = true;
float ClockTime = 0;

int handleBuffer(int bytesNeeded, int curBSIZE);
void Arrival(const Process& process, vector<ProcessEntry>& processTable, const vector<Input>& inputTable);
void CoreRequest(Process& process, vector<ProcessEntry>& pTable, const vector<Input>& inputTable);
void SSDRequest(Process& process, vector<ProcessEntry>& processTable, const vector<Input>& inputTable);
void CoreCompletion(Process& process, vector<ProcessEntry>& processTable, const vector<Input>& inputTable);
void SSDCompletion(Process& process, vector<ProcessEntry>& processTable, const vector<Input>& inputTable);

vector<Input> processInputFileToTable(const string& fileName, vector<ProcessEntry>& processTable) {
    ifstream file(fileName);
    string line;
    int starts = 0;
    vector<Input> inputTable;
    int lineCounter = 0;
    bool newProcess = false;
    ProcessEntry currentProcess;

    while (getline(file, line)) {
        istringstream iss(line);
        Input input;
        iss >> input.command >> input.time;

        inputTable.push_back(input); // Populate the input table

        if(input.command == "BSIZE")
                BSIZE = input.time;

        if (input.command == "START") {
            if (newProcess) { // If already in a process, finalize it
                currentProcess.EndLine = lineCounter - 1;
                processTable.push_back(currentProcess);
            }
            newProcess = true;
            currentProcess = ProcessEntry(); // Start a new process entry
            currentProcess.PID = starts; // Increment PID for new process
            starts ++;
            currentProcess.StartLine = lineCounter;
        }
        currentProcess.EndLine = lineCounter; // Update as we go
        lineCounter++; // Increment lineCounter after processing the line
    }
    // Add the last process if it exists
    if (newProcess) {
        processTable.push_back(currentProcess);
    }

    return inputTable;
}
vector<Input> processInputFromCin(vector<ProcessEntry>& processTable) {
    vector<Input> inputTable;
    string line;
    int starts = 0;
    int lineCounter = 0;
    bool newProcess = false;
    ProcessEntry currentProcess;

    while (getline(cin, line)) {
        istringstream iss(line);
        Input input;
        if (!(iss >> input.command >> input.time)) break; // Break if input is invalid

        inputTable.push_back(input);

        if(input.command == "BSIZE")
            BSIZE = input.time;
    
        if (input.command == "START") {
            if (newProcess) { // If already in a process, finalize it
                currentProcess.EndLine = lineCounter - 1;
                processTable.push_back(currentProcess);
            }
            newProcess = true;
            currentProcess = ProcessEntry(); // Start a new process entry
            currentProcess.PID = starts; // Increment PID for new process
            starts ++;
            currentProcess.StartLine = lineCounter;
        }
        currentProcess.EndLine = lineCounter; // Update as we go
        lineCounter++; // Increment lineCounter after processing the line
    }
    // Add the last process if it exists
    if (newProcess) {
    processTable.push_back(currentProcess);
    }

    return inputTable;
}
void printInputTable(const vector<Input>& table) {
    cout << "Index Com. Time/Value" << endl;
    for (size_t i = 0; i < table.size(); i++) {
        cout << i << " " << table[i].command << " " << table[i].time << endl;
    }
}
void printProcessTable(const vector<ProcessEntry>& table) {
    cout << "PID; Start Line; End Line; Current Line; State;" << endl;
    for (const auto& entry : table) {
        cout << entry.PID << "; " << entry.StartLine << "; " << entry.EndLine << "; "
             << entry.CurrentLine << "; " << entry.State << ";" << endl;
    }
}
void printMainQueue() {
    cout << "Main Queue Contents:" << endl;
    while (!MainQueue.empty()) {
        Process process = MainQueue.top();
        MainQueue.pop();
        cout << "Process " << process.PID << ":" << endl;
        cout << "  Time Completion: " << process.TimeCompletion << endl;
        cout << "  Instruction: " << process.Instruction << endl;
        cout << "  NumLogReads: " << process.NumLogReads << endl;
        cout << "  NumPhyWrites: " << process.NumPhyWrites << endl;
        cout << "  NumPhyReads: " << process.NumPhyReads << endl;
        cout << "  BufferSize: " << process.BufferSize << endl;
    }
}

int main() {
    vector<ProcessEntry> processTable;
    //use these entries to hard-code files into the program
    //const string fileName = "input10.txt";
    //vector<Input> inputTable = processInputFileToTable(fileName, processTable);
    vector<Input> inputTable = processInputFromCin(processTable);

    if(verboseLog){
        cout << "Input Table:" << endl;
        printInputTable(inputTable);
        cout << "\nProcess Table:" << endl;
        printProcessTable(processTable);
        cout << "\n";
    }
    for (const auto& entry : processTable) {
        // Retrieve the start line index of the process entry
        int startLineIndex = entry.StartLine;

        string command = inputTable[startLineIndex].command;
        float time = inputTable[startLineIndex].time;

        Process process;
        process.PID = entry.PID;
        process.TimeCompletion = time;
        process.Instruction = command;

        MainQueue.push(process);
    }
    
    // Main simulation loop
    while (!MainQueue.empty()) {
        if(verboseLog) cout << "main queue entered, looking at top process. It is: ";
        Process currentProcess = MainQueue.top();
        MainQueue.pop();
        if(verboseLog) cout << currentProcess.PID << " " << currentProcess.Instruction << endl;
        if (currentProcess.TimeCompletion > ClockTime) {
            ClockTime = currentProcess.TimeCompletion;
        }

        if (currentProcess.Instruction == "START") {
            Arrival(currentProcess, processTable, inputTable);
        } else if (currentProcess.Instruction == "CORE") {
            CoreCompletion(currentProcess, processTable, inputTable);
        } else {
            SSDCompletion(currentProcess, processTable, inputTable);
        }
    }
    return 0;
}

int handleBuffer(int bytesNeeded, int curBSIZE) {
    if(verboseLog) cout << "\nhandle buffer called. Bytes needed: " << bytesNeeded << " Current Buffer Size: " << curBSIZE << endl;
    int finalBufferSize;
    if (bytesNeeded <= curBSIZE) {
        finalBufferSize = curBSIZE - bytesNeeded;
    } else {
        int bytesMissing = bytesNeeded - curBSIZE;
        int bytesBrought;
        if (bytesMissing % BSIZE == 0) {
            bytesBrought = bytesMissing;
        } else {
            bytesBrought = ((bytesMissing / BSIZE) + 1) * BSIZE;
        }
        finalBufferSize = curBSIZE + bytesBrought - bytesNeeded;
    }
    if(verboseLog) cout << "Final buffer size: " << finalBufferSize << endl << endl;
    return finalBufferSize;
}

void Arrival(const Process& process, vector<ProcessEntry>& processTable, const vector<Input>& inputTable) {
    // Update the current line of the process entry in the process table
    processTable[process.PID].CurrentLine = processTable[process.PID].StartLine + 1;

    string command = inputTable[processTable[process.PID].CurrentLine].command;
    float time = inputTable[processTable[process.PID].CurrentLine].time;
    if(verboseLog) cout << "arrival command/time : " << command << " " << time << endl;

    // Update the process with the new command and time
    Process updatedProcess = process;
    updatedProcess.Instruction = command;
    updatedProcess.TimeCompletion = time;

    // Call the Core Request function with the updated process and process table
   CoreRequest(updatedProcess, processTable, inputTable);
}

void CoreRequest(Process& process, vector<ProcessEntry>& pTable, const vector<Input>& inputTable) {
    if(verboseLog) cout << "CoreRequest called with " << process.PID << " " << process.Instruction << endl;
    // process.PID is used to find the corresponding entry in pTable
    int pidIndex = -1;
    for (size_t i = 0; i < pTable.size(); ++i) {
        if (pTable[i].PID == process.PID) {
            pidIndex = static_cast<int>(i);
            break;
        }
    }

    // [A] If the CORE is empty
    if (CpuIsEmpty) {
        if(verboseLog) cout << "CPU empty, process " << process.PID << " is moved to the CPU(main) queue" << endl;
        CpuIsEmpty = false; // Set the CPU to full
        pTable[pidIndex].State = 1; // Change pTable status to Running
        if(verboseLog) cout << "\n\n process.TimeCompletion: " << process.TimeCompletion << "\n\n";
        float operationDuration = process.TimeCompletion; // Duration of the CORE operation
        process.TimeCompletion = ClockTime + operationDuration; // Scheduling completion

        MainQueue.push(process);
    }
    // [B] If the Core is Full
    else {
        if(verboseLog) cout << "CPU full, process " << process.PID << " is moved to the ready queue" << endl;
        pTable[pidIndex].State = 2; // Change the status of the event to ready
        RQ.push(process);
    }
}

void SSDRequest(Process& process, vector<ProcessEntry>& processTable, const vector<Input>& inputTable) {
    if(verboseLog) cout << "SSDRequest reached with " << process.PID << " " << process.Instruction << endl;
    float ssdOperationDuration = 0.1; // Fixed duration for SSD operations
    if (process.Instruction == "READ") {
        // Determine if read is physical or logical
        if (inputTable[processTable[process.PID].CurrentLine].time >= process.BufferSize) {
            //physical read, get from buffer, increase curBSIZE
            if(verboseLog) cout << "physical read, push to main queue or SSD queue if SSD full\n" << endl;
            
            if (SSDisEmpty) {
                process.BufferSize = handleBuffer(inputTable[processTable[process.PID].CurrentLine].time, process.BufferSize);
                process.TimeCompletion = ClockTime + ssdOperationDuration;
                process.NumPhyReads++;
                SSDisEmpty = false;
                MainQueue.push(process); // Directly process as SSD is empty
            } else {
                SQ.push(process); // SSD is full, push to SSD queue
            }
        } else {
            // Logical Read, remove read bits from curBSIZE
            if(verboseLog) cout << "logical read, core request called for next command\n" << endl;
            process.NumLogReads++;
            process.BufferSize = handleBuffer(inputTable[processTable[process.PID].CurrentLine].time, process.BufferSize);
            // these lines are just for moving to the next index
            processTable[process.PID].CurrentLine++;
            string command = inputTable[processTable[process.PID].CurrentLine].command;
            float time = inputTable[processTable[process.PID].CurrentLine].time;
            process.TimeCompletion = time;
            process.Instruction = command;
            //
            CoreRequest(process, processTable, inputTable);
        }
    } else if (process.Instruction == "WRITE") {
        // Writes are always physical
        if(verboseLog) cout << "physical write, push to main queue or SSD queue if SSD full\n";
        if (SSDisEmpty) {
            if(verboseLog) cout << "pushed to main queue\n" << endl;
            process.NumPhyWrites++;
            process.TimeCompletion = ClockTime + ssdOperationDuration;
            SSDisEmpty = false; // Mark SSD as in use
            MainQueue.push(process); // Directly process as SSD is empty
        } else {
            if(verboseLog) cout << "pushed to SSD queue\n" << endl;
            SQ.push(process); // SSD is full, push to SSD queue
        }
    }
}

void CoreCompletion(Process& process, vector<ProcessEntry>& processTable, const vector<Input>& inputTable) {
    if(verboseLog){
        cout << "\nCore Completion called with " << process.PID << " " << process.Instruction << endl;
        cout << "process info-- " << "GLOBAL clock time: " << ClockTime << " " << "completion time: " << process.TimeCompletion << endl;
        cout << "logReads: " << process.NumLogReads << " phyReads: " << process.NumPhyReads << " phyWrites: " << process.NumPhyWrites << "\n";
    }
    CpuIsEmpty = true; // Set the Core to empty
    if (!RQ.empty()) {
        if(verboseLog) cout << "there is a process inthe SSD queue, sending to CoreRequest" << endl;
        // If RQ is not empty, schedule the next process waiting in RQ
        Process nextProcess = RQ.front();
        if(verboseLog) cout << "waiting process: " << nextProcess.PID << " " << nextProcess.Instruction << endl << endl;
        RQ.pop();
        CoreRequest(nextProcess, processTable, inputTable);
    }

    // Retrieve the process entry from the process table
    auto& entry = processTable[process.PID]; // Assuming direct access by PID for simplicity
    if(verboseLog) cout << "entry data: " << entry.PID << " " << entry.StartLine << " " << entry.EndLine << endl;
    if (entry.CurrentLine == entry.EndLine) {
        // If End Line == Current Line, we terminate the process
        //update to state 4 for terminated, print num of writes and reads
        entry.State = 4;
        cout << "\nProcess " << process.PID << " terminates at " << ClockTime << " ms" << endl;
        cout << "physical reads: [" << process.NumPhyReads << "] logical reads: [" << process.NumLogReads << "] physical writes: [" << process.NumPhyWrites << "]" << endl;
        //print each process and its state
        cout << "--------------" << endl; 
        cout << "Process Table:" << endl;
        map<int, string> stateMap = {
            {0, "Null"},
            {1, "Running"},
            {2, "Ready"},
            {3, "Blocked"},
            {4, "Terminated"}
        };
        for (const auto& entry : processTable) {
            cout << "Process " << entry.PID << ": " << stateMap[entry.State] << endl;
        }
        if(verboseLog) cout << "\n";
        return; 
    } else {
        // Increment the current line by 1 for the next instruction
        processTable[process.PID].CurrentLine++;
        string command = inputTable[processTable[process.PID].CurrentLine].command;
        float time = inputTable[processTable[process.PID].CurrentLine].time;
        process.TimeCompletion = time;
        process.Instruction = command;
        if(verboseLog) cout << "process instruction:: " << process.Instruction << " and time: " << process.TimeCompletion << endl;
        if (process.Instruction == "READ" || process.Instruction == "WRITE") {
            if(verboseLog) cout << "write or read was taken, go to ssd request\n\n" << endl;
            SSDRequest(process, processTable, inputTable);
        } else if (process.Instruction == "INPUT" || process.Instruction == "DISPLAY") {
            if(verboseLog) cout << "I/O, push to mainqueue" << endl;
            if(verboseLog) cout << "time duration for I/O operation: " << process.TimeCompletion << endl << endl;
            process.TimeCompletion = ClockTime + process.TimeCompletion;
            MainQueue.push(process);
        }
    }
}

void SSDCompletion(Process& process, vector<ProcessEntry>& processTable, const vector<Input>& inputTable) {
    if(verboseLog) cout << "SSD Completion reached with " << process.PID << " " << process.Instruction << endl;
    SSDisEmpty = true; // Mark the SSD as empty

    if (!SQ.empty()) { // If there is a process waiting in the SSD queue
        if(verboseLog) cout << "there is a process in the SSD queue, sending to SSD request" << endl;
        Process nextProcess = SQ.front();
        SQ.pop();
        if(verboseLog) cout << "waiting process: " << nextProcess.PID << " " << nextProcess.Instruction << endl << endl;
        SSDRequest(nextProcess, processTable, inputTable); // Send the next process to the SSD request function
    }
    // proceed with the completed process's next command
    processTable[process.PID].CurrentLine++;
    string command = inputTable[processTable[process.PID].CurrentLine].command;
    float time = inputTable[processTable[process.PID].CurrentLine].time;
    process.TimeCompletion = time;
    process.Instruction = command;
    CoreRequest(process, processTable, inputTable); // Send the process to the Core request function
}