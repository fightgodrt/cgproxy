#include "cgroup_attach.h"
#include "common.h"
#include <errno.h>
#include <fstream>
#include <iostream>
#include <regex>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

namespace CGPROXY::CGROUP {

string cgroup2_mount_point = get_cgroup2_mount_point();

string get_cgroup2_mount_point() {
  stringstream buffer;
  FILE *fp = popen("findmnt -t cgroup2 -n -o TARGET", "r");
  if (!fp) return "";
  char buf[READ_SIZE_MAX];
  while (fgets(buf, READ_SIZE_MAX, fp) != NULL) { buffer << buf; }
  pclose(fp);
  string s = buffer.str();
  s.pop_back(); // remove newline character
  return s;
}

string getCgroup(const pid_t &pid) { return getCgroup(to_str(pid)); }

string getCgroup(const string &pid) {
  string cgroup_f = to_str("/proc/", pid, "/cgroup");
  if (!fileExist(cgroup_f)) return "";

  stringstream buffer;
  string cgroup;
  FILE *f = fopen(cgroup_f.c_str(), "r");
  char buf[READ_SIZE_MAX] = "";
  char *flag = buf;
  while (flag != NULL) {
    buffer.clear();
    while (!flag || buf[strlen(buf) - 1] != '\n') {
      flag = fgets(buf, READ_SIZE_MAX, f);
      if (flag) buffer << buf;
    }
    string line = buffer.str();
    if (line[0] == '0') { // 0::/user.slice/user-1000.slice
      cgroup = (*(line.end() - 1) == '\n') ? line.substr(3, line.length() - 4)
                                           : line.substr(3);
      break;
    }
  }
  fclose(f);
  return cgroup;
}

bool validate(string pid, string cgroup) {
  bool pid_v = validPid(pid);
  bool cg_v = validCgroup(cgroup);
  if (pid_v && cg_v) return true;

  error("attach paramater validate error");
  return_error;
}

int attach(const string pid, const string cgroup_target) {
  if (getuid() != 0) {
    error("need root to attach cgroup");
    return_error;
  }

  debug("attaching %s to %s", pid.c_str(), cgroup_target.c_str());

  if (!validate(pid, cgroup_target)) return_error;
  if (cgroup2_mount_point.empty()) return_error;
  string cgroup_target_path = cgroup2_mount_point + cgroup_target;
  string cgroup_target_procs = cgroup_target_path + "/cgroup.procs";

  // check if exist, we will create it if not exist
  if (!dirExist(cgroup_target_path)) {
    if (mkdir(cgroup_target_path.c_str(),
              S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) == 0) {
      debug("created cgroup %s success", cgroup_target.c_str());
    } else {
      error("created cgroup %s failed, errno %d", cgroup_target.c_str(), errno);
      return_error;
    }
    // error("cgroup %s not exist",cgroup_target.c_str());
    // return_error
  }

  if (getCgroup(pid) == cgroup_target) {
    debug("%s already in %s", pid.c_str(), cgroup_target.c_str());
    return_success;
  }

  // put pid to target cgroup
  ofstream procs(cgroup_target_procs, ofstream::app);
  if (!procs.is_open()) {
    error("open file %s failed", cgroup_target_procs.c_str());
    return_error;
  }
  procs << pid.c_str() << endl;
  procs.close();

  // maybe there some write error, for example process pid may not exist
  if (!procs) {
    error("write %s to %s failed, maybe process %s not exist", pid.c_str(),
          cgroup_target_procs.c_str(), pid.c_str());
    return_error;
  }
  return_success;
}

int attach(const int pid, const string cgroup_target) {
  return attach(to_str(pid), cgroup_target);
}

} // namespace CGPROXY::CGROUP
