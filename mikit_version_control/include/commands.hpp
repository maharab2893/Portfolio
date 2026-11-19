#pragma once

namespace mikit {

int cmd_init(int argc, char* argv[]);
int cmd_add(int argc, char* argv[]);
int cmd_status(int argc, char* argv[]);
int cmd_commit(int argc, char* argv[]);
int cmd_cat_file(int argc, char* argv[]);
int cmd_log(int argc, char* argv[]);
int cmd_config(int argc, char* argv[]);

} // namespace mikit