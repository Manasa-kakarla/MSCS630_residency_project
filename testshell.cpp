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

using namespace std;

// Structure to represent a job in the background
struct Job {
    pid_t pid;
    string command;
    bool stopped;
};

// Job list for background processes
vector<Job> jobList;

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
        } else {
            waitpid(pid, NULL, 0);  // Wait for the foreground process to finish
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

