#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <cstring>
#include <dirent.h>
#include <stdlib.h>
#include <fstream>
#include <sys/stat.h>
#include <queue>
#include <functional>
#include <thread>  // For simulating sleep
#include <chrono>  // For sleep duration

using namespace std;
using namespace chrono;

// Structure to represent a job in the background
struct Job {
    pid_t pid;
    string command;
    bool stopped;
    bool isRunning;
    int priority;  // Priority for Priority Scheduling
    int remainingTime;  // For round-robin scheduling
    time_point<steady_clock> arrivalTime;  // Time when the process arrived
    time_point<steady_clock> startTime;   // Time when the process first starts execution
    time_point<steady_clock> completionTime;  // Time when the process completes execution
    int waitingTime;  // Time spent waiting in the queue
    int turnaroundTime;  // Total time spent from arrival to completion
    int responseTime;  // Time from arrival to first execution
};

// Job list for background processes
vector<Job> jobList;
int timeSlice = 3;  // Default time slice for round-robin scheduling (in seconds)

// Function to execute a command in the background
void executeCommandInBackground(const std::vector<std::string>& args) {
    pid_t pid = fork();
    if (pid == 0) {
        // Child process
        const char* cmd = args[0].c_str();
        char* argv[args.size() + 1];  // +1 for null terminator
        for (size_t i = 0; i < args.size(); ++i) {
            argv[i] = const_cast<char*>(args[i].c_str());
        }
        argv[args.size()] = nullptr;

        execvp(cmd, argv);  // Execute the command

        // If execvp fails
        cerr << "Error: Unable to execute command." << endl;
        exit(1);
    } else if (pid > 0) {
        // Parent process
        // Create the job object explicitly
        Job job;
        job.pid = pid;
        job.command = args[0];   // Store the command
        job.stopped = false;
        job.isRunning = true;
        job.remainingTime = timeSlice; // Assuming the time slice is defined
        job.arrivalTime = steady_clock::now();
        
        // Push the job into the job list
        jobList.push_back(job);

        cout << "Job with PID " << pid << " started in the background." << endl;
    } else {
        cerr << "Fork failed!" << endl;
    }
}

void printPerformanceMetrics();

// Round-Robin Scheduling Algorithm
void roundRobinScheduling() {
    cout << "Round-Robin Scheduling started with a time slice of " << timeSlice << " seconds." << endl;

    while (!jobList.empty()) {
	size_t jobCount = jobList.size();  // Avoid modifying the list while iterating
        for (size_t i = 0; i < jobCount; ++i) {
            Job& process = jobList[i];

            if (!process.isRunning) {
    		cout << "Round-Robin entered the if loop." << endl;
                continue;  // Skip if process has finished
            }

	    // If the job has just started, set the response time
            if (process.startTime == time_point<steady_clock>()) {
                process.startTime = steady_clock::now();
                process.responseTime = duration_cast<seconds>(process.startTime - process.arrivalTime).count();
            }

            cout << "Executing PID " << process.pid << " for " << timeSlice << " seconds." << endl;
            this_thread::sleep_for(seconds(timeSlice));  // Simulate process running for the time slice

            process.remainingTime -= timeSlice;
	    cout << "Process " << process.pid << " remaining time: " << process.remainingTime << " seconds." << endl;

            if (process.remainingTime <= 0) {
                process.isRunning = false;  // Process completed
		process.remainingTime = 0;  // Set remaining time to zero after completion
		process.completionTime = steady_clock::now();
                process.turnaroundTime = duration_cast<seconds>(process.completionTime - process.arrivalTime).count();
                process.waitingTime = process.turnaroundTime - (timeSlice);

                cout << "Process " << process.pid << " completed." << endl;
            }
        }

        // Remove completed processes from job list
        jobList.erase(remove_if(jobList.begin(), jobList.end(),
            [](const Job& p) { return !p.isRunning; }), jobList.end());

    	// Print performance metrics after scheduling
    	printPerformanceMetrics();

	cout << "Remaining jobs in the list: " << jobList.size() << endl;
    }
    cout << "Round-Robin Scheduling completed." << endl;
}

void printPerformanceMetrics() {
   // bool anyJobPrinted = false;
    for (const auto& process : jobList) {
        if (!process.isRunning) {
	    // Print performance metrics for completed jobs
            cout << "Process PID: " << process.pid << endl;
	    cout << "Command: " << process.command << endl;
            cout << "Waiting Time: " << process.waitingTime << " seconds" << endl;
            cout << "Turnaround Time: " << process.turnaroundTime << " seconds" << endl;
            cout << "Response Time: " << process.responseTime << " seconds" << endl;
	    //anyJobPrinted = true;
        }
    }
    /*if (!anyJobPrinted) {
        cout << "No completed jobs to display performance metrics." << endl;
    }*/
}

// Priority-Based Scheduling Algorithm
void priorityScheduling() {
    cout << "Priority-Based Scheduling started." << endl;

    // Using a priority queue to select the highest priority process first
    auto cmp = [](const Job& a, const Job& b) {
        if (a.priority == b.priority) {
            return a.arrivalTime > b.arrivalTime;  // FCFS if priority is the same
        }
        return a.priority < b.priority;  // Higher priority comes first
    };

    priority_queue<Job, vector<Job>, decltype(cmp)> pq(cmp);

    // Add all jobs to the priority queue
    for (auto& process : jobList) {
        pq.push(process);
    }

    while (!pq.empty()) {
        Job process = pq.top();
        pq.pop();

        if (!process.isRunning) {
            continue;  // Skip if process has finished
        }

	// If the job has just started, set the response time
        if (process.startTime == time_point<steady_clock>()) {
            process.startTime = steady_clock::now();
            process.responseTime = duration_cast<seconds>(process.startTime - process.arrivalTime).count();
        }

        cout << "Executing PID " << process.pid << " (Priority " << process.priority << ")." << endl;
        this_thread::sleep_for(seconds(timeSlice));  // Simulate process running for the time slice

        process.remainingTime -= timeSlice;
        if (process.remainingTime <= 0) {
            process.isRunning = false;  // Process completed
            process.completionTime = steady_clock::now();
            process.turnaroundTime = duration_cast<seconds>(process.completionTime - process.arrivalTime).count();
            process.waitingTime = process.turnaroundTime - (process.remainingTime + timeSlice);
	    cout << "Process " << process.pid << " completed." << endl;
        }

        // Re-add process to the queue if it's not finished
        if (process.isRunning) {
            pq.push(process);
        }
    }

    cout << "Priority-Based Scheduling completed." << endl;
    // Print performance metrics after scheduling
    printPerformanceMetrics();
}

// Function to execute built-in commands
bool executeBuiltInCommand(const vector<string>& args) {
    if (args[0] == "exit") {
        exit(0);
    } else if (args[0] == "pwd") {
        char cwd[1024];
        if (getcwd(cwd, sizeof(cwd)) != NULL) {
            cout << cwd << endl;
        } else {
            perror("pwd");
        }
        return true;
    } else if (args[0] == "cd") {
        if (args.size() < 2) {
            cerr << "cd: missing argument" << endl;
        } else if (chdir(args[1].c_str()) != 0) {
            perror("cd");
        }
        return true;
    } else if (args[0] == "clear") {
        cout << "\033[H\033[J";  // ANSI escape sequence to clear the terminal
        return true;
    } else if (args[0] == "echo") {
        for (size_t i = 1; i < args.size(); ++i) {
            cout << args[i] << " ";
        }
        cout << endl;
        return true;
    } else if (args[0] == "jobs") {
        if (jobList.empty()) {
            cout << "No background jobs." << endl;
        } else {
            for (size_t i = 0; i < jobList.size(); ++i) {
                cout << "[" << i + 1 << "] PID: " << jobList[i].pid << " Command: " << jobList[i].command;
                if (jobList[i].stopped) {
                    cout << " (stopped)";
                }
                cout << endl;
            }
        }
        return true;
    } else if (args[0] == "ls") {
        DIR *dir = opendir(".");
        struct dirent *entry;
        if (dir == NULL) {
            perror("ls");
            return true;
        }

        while ((entry = readdir(dir)) != NULL) {
            cout << entry->d_name << endl;
        }
        closedir(dir);
        return true;
    } else if (args[0] == "cat") {
        if (args.size() < 2) {
            cerr << "cat: missing file argument" << endl;
            return true;
        }
        ifstream file(args[1]);
        if (!file.is_open()) {
            perror("cat");
            return true;
        }
        string line;
        while (getline(file, line)) {
            cout << line << endl;
        }
        file.close();
        return true;
    } else if (args[0] == "mkdir") {
        if (args.size() < 2) {
            cerr << "mkdir: missing directory name" << endl;
            return true;
        }
        if (mkdir(args[1].c_str(), 0777) != 0) {
            perror("mkdir");
        }
        return true;
    } else if (args[0] == "rmdir") {
        if (args.size() < 2) {
            cerr << "rmdir: missing directory name" << endl;
            return true;
        }
        if (rmdir(args[1].c_str()) != 0) {
            perror("rmdir");
        }
        return true;
    } else if (args[0] == "rm") {
        if (args.size() < 2) {
            cerr << "rm: missing file name" << endl;
            return true;
        }
        if (remove(args[1].c_str()) != 0) {
            perror("rm");
        }
        return true;
    } else if (args[0] == "touch") {
        if (args.size() < 2) {
            cerr << "touch: missing file name" << endl;
            return true;
        }
        ofstream file(args[1]);
        if (!file) {
            perror("touch");
        }
        file.close();
        return true;
    } else if (args[0] == "kill") {
        if (args.size() < 2) {
            cerr << "kill: missing PID argument" << endl;
            return true;
        }
        pid_t pid = stoi(args[1]);
        if (kill(pid, SIGTERM) != 0) {
            perror("kill");
        }
        return true;
    }
    return false;
}

void removeDeadJobs() {
    int status;
    for (auto it = jobList.begin(); it != jobList.end(); ) {
        pid_t result = waitpid(it->pid, &status, WNOHANG);  // Non-blocking wait
        if (result == it->pid) {  // Process finished
            std::cout << "Job " << it->pid << " (" << it->command << ") finished." << std::endl;
            it = jobList.erase(it);  // Remove from job list
        } else {
            ++it;  // Keep checking other jobs
        }
    }
}

void listJobs() {
    removeDeadJobs();  // Clean up finished jobs before displaying
    for (const auto& job : jobList) {
        std::cout << "[" << job.pid << "] PID: " << job.pid << " Command: " << job.command << std::endl;
    }
}

// Function to kill a background job
void killJob(int jobId) {
    bool found = false;
    for (auto& job : jobList) {
        if (job.pid == jobId) {
            kill(job.pid, SIGTERM);  // Send SIGTERM to terminate the job
            found = true;
            break;
        }
    }
    if (!found) {
        std::cout << "Invalid job ID." << std::endl;
    }
}

// Function to execute external commands
void executeCommand(const vector<string>& args, bool isBackground) {
   
    // Handle internal commands like 'round-robin' and 'priority-scheduling' directly
    if (args[0] == "round-robin") {
        if (args.size() == 2) {
            timeSlice = stoi(args[1]);  // Update time slice if provided
        }
        roundRobinScheduling();  // Call the Round-Robin scheduling function
        return;  // No need to proceed further with execvp
    } else if (args[0] == "priority-scheduling") {
        priorityScheduling();  // Call the Priority Scheduling function
        return;  // No need to proceed further with execvp
    }

    // If not an internal command, proceed with the execvp logic
    pid_t pid = fork();
    
    if (pid == -1) {
        perror("fork");
        return;
    }

    if (pid == 0) { // Child process
        vector<char*> c_args;
        for (const auto& arg : args) {
            c_args.push_back(const_cast<char*>(arg.c_str()));
        }
        c_args.push_back(NULL);  // Null-terminate the argument list

        if (execvp(c_args[0], c_args.data()) == -1) {
            perror("execvp");
            exit(1);
        }
    } else { // Parent process
        if (isBackground) {
            // Add the job to the background list
            jobList.push_back({pid, args[0], false});
	    cout << "Job with PID " << pid << " started in the background." << endl;
        } else {
            waitpid(pid, NULL, 0);  // Wait for the foreground process to finish
            cout << "Job with PID " << pid << " completed in the foreground." << endl;
	}
    }
}

// Function to bring a background job to the foreground
void bringJobToForeground(int job_id) {
    if (job_id <= 0 || job_id > jobList.size()) {
        cerr << "Invalid job ID." << endl;
        return;
    }
    Job& job = jobList[job_id - 1];
    
    if (job.stopped) {
        kill(job.pid, SIGCONT);  // Send SIGCONT to resume the job
        job.stopped = false;
    }

    waitpid(job.pid, NULL, 0);  // Wait for the job to finish in the foreground
    jobList.erase(jobList.begin() + job_id - 1);  // Remove job from list
}

// Function to resume a stopped background job
void resumeJobInBackground(int job_id) {
    if (job_id <= 0 || job_id > jobList.size()) {
        cerr << "Invalid job ID." << endl;
        return;
    }
    Job& job = jobList[job_id - 1];
    
    if (!job.stopped) {
        cerr << "Job is already running in the background." << endl;
        return;
    }

    kill(job.pid, SIGCONT);  // Send SIGCONT to resume the job
    job.stopped = false;
}

// Function to parse input into commands and arguments
vector<string> parseInput(const string& input) {
    vector<string> args;
    stringstream ss(input);
    string word;
    
    while (ss >> word) {
        args.push_back(word);
    }

    return args;
}

// Main shell loop
int main() {
    string input;
    while (true) {
        cout << "> ";
        getline(cin, input);

        if (input.empty()) continue;

        vector<string> args = parseInput(input);
        bool isBackground = false;

        // Check if background process is requested (ends with '&')
        if (args.back() == "&") {
            isBackground = true;
            args.pop_back();  // Remove '&'
        }

        // Execute built-in commands
        if (executeBuiltInCommand(args)) {
            continue;
        }
	
	// Handle Round-Robin or Priority Scheduling
	if (args[0] == "round-robin") {
            if (args.size() == 2) {
                timeSlice = stoi(args[1]);  // Update time slice
            }
            roundRobinScheduling();
        } else if (args[0] == "priority-scheduling") {
            priorityScheduling();
        } else {
            // Execute command in the background
            executeCommandInBackground(args);
        }

        // Handle foreground and background command execution
        if (args[0] == "fg") {
            if (args.size() == 2) {
                int job_id = stoi(args[1]);
                bringJobToForeground(job_id);
            } else {
                cerr << "Usage: fg <job_id>" << endl;
            }
        } else if (args[0] == "bg") {
            if (args.size() == 2) {
                int job_id = stoi(args[1]);
                resumeJobInBackground(job_id);
            } else if (input.substr(0, 2) == "kill") {
            	int jobId = std::stoi(input.substr(5));
            	killJob(jobId); 
	    } else {
                cerr << "Usage: bg <job_id>" << endl;
            }
        } else {
            executeCommand(args, isBackground);
        }
    }

    return 0;
}

