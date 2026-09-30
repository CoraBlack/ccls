#include <linux/limits.h>
#include <stddef.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>

typedef struct {
    bool a;
    bool A;

    bool l;
    bool L;
} LsOptions;

LsOptions default_option() {
    LsOptions opt = {
        .a = false,
        .A = false,

        .l = false,
        .L = false,
    };

    return opt;
}

void parse_args(int argc, char* argv[], LsOptions* opt, char** dir_list[], uint32_t* dir_count);
bool parse_flag_token(const char* token, LsOptions* opt);
void normalize_opt(LsOptions* opt);
void print_dir(char* dir_path, LsOptions* opt);

// Utils function
size_t get_user_name(uid_t uid, char* name_buf[]);
size_t get_group_name(gid_t gid, char* name_buf[]);

int main(int argc, char* argv[]) {
    char** dirs = NULL;
    uint32_t dir_count = 0;
    LsOptions opt = default_option();

    errno = 0;
    parse_args(argc, argv, &opt, &dirs, &dir_count);
    normalize_opt(&opt);

    if (dir_count == 0) {
        print_dir(".", &opt);
    } else {
        for (uint32_t i = 0; i < dir_count; ++i) {
            char* dir_path = dirs[i];

            if (dir_count > 1) printf("%s:\n", dir_path);
            print_dir(dir_path, &opt);
            if (dir_count > 1) printf("\n");
        }
    }

    free(dirs);
    return 0;
}

void parse_args(int argc, char* argv[], LsOptions* opt, char** dir_list[], uint32_t* dir_count) {
    *dir_count = 0;
    char** dirs = malloc(argc * sizeof(char*));
    memset(dirs, 0, argc * sizeof(char*));

    for (uint32_t i = 1; i < argc; ++i) {
        char* token = argv[i];

        switch (token[0]) {
            case '-': {
                bool err = parse_flag_token(token, opt);
                if (!err) {
                    errno = EINVAL;
                    return;
                }
                break;
            }

            default: {
                dirs[*dir_count] = token;
                *dir_count += 1;
                break;
            }
        }
    }

    *dir_list = dirs;
}

bool parse_flag_token(const char* token, LsOptions* opt) {
    if (token[0] != '-') return false;

    for (uint32_t i = 1; i < strlen(token); ++i) {
        const char c = token[i];
        switch (c) {
            case 'a': {
                opt->a = true;
                break;
            }

            case 'A': {
                opt->A = true;
                break;
            }

            case 'l': {
                opt->l = true;
                break;
            }

            case 'L': {
                opt->L = true;
                break;
            }

            default: {
                return false;
            }
        }
    }

    return true;
}

void normalize_opt(LsOptions* opt) {
    if (opt->a) opt->A = true;
}

void print_dir(char* dir_path, LsOptions* opt) {
    if (dir_path == NULL) return;

    errno = 0;
    DIR* dir = opendir(dir_path);
    if (errno != 0 && dir == NULL) {
        perror("opendir");
        return;
    }

    for (struct dirent* entry = readdir(dir); entry != NULL; entry = readdir(dir)) {
        const char* entry_name = entry->d_name;

        if (strcmp(entry_name, ".") == 0 || strcmp(entry_name, "..") == 0) {
            if (!opt->a) continue;
        } else if (entry_name[0] == '.') {
            if (!opt->A) continue;
        }

        struct stat stat_buf;
        char full_entry_path[PATH_MAX + 1] = {0};
        sprintf(full_entry_path, "%s/%s", dir_path, entry_name);
        errno = 0;
        if (lstat(full_entry_path, &stat_buf) != 0) {
            perror("stat");
            goto CLEAN;
        }

        mode_t stat_mode = stat_buf.st_mode;

        // display with list style
        if (opt->l) {
            char* name_buf = malloc(1024 * sizeof(char));
            memset(name_buf, 0, 1024 * sizeof(char));

            size_t n1 = get_user_name(stat_buf.st_uid, &name_buf);
            char user_name[n1 + 1];
            strcpy(user_name, name_buf);

            size_t n2 = get_group_name(stat_buf.st_gid, &name_buf);
            char group_name[n2 + 1];
            strcpy(group_name, name_buf);

            printf("%c%c%c%c%c%c%c%c%c %s %s %s",
                   (stat_mode & S_IRUSR) ? 'r' : '-',
                   (stat_mode & S_IWUSR) ? 'w' : '-',
                   (stat_mode & S_IXUSR) ? 'x' : '-',
                   (stat_mode & S_IRGRP) ? 'r' : '-',
                   (stat_mode & S_IWGRP) ? 'w' : '-',
                   (stat_mode & S_IXGRP) ? 'x' : '-',
                   (stat_mode & S_IROTH) ? 'r' : '-',
                   (stat_mode & S_IWOTH) ? 'w' : '-',
                   (stat_mode & S_IXOTH) ? 'x' : '-',
                   group_name,
                   user_name,
                   entry_name
            );

            if (S_ISLNK(stat_buf.st_mode)) {
                char link_target[PATH_MAX + 1] = {0};
                ssize_t len = readlink(full_entry_path, link_target, PATH_MAX);
                if (len >= 0) {
                    link_target[len] = '\0';
                }
                printf(" -> %s", link_target);
            }

            printf("\n");

            free(name_buf);
        } else {
            if (S_ISLNK(stat_buf.st_mode)) printf("@");
            printf("%s\t", entry_name);
        }
    }

CLEAN:
    closedir(dir);
}

size_t get_user_name(uid_t uid, char* name_buf[]) {
    size_t name_len = 0;
    struct passwd pwd_buf;
    struct passwd* result = NULL;
    size_t buf_len = sysconf(_SC_GETPW_R_SIZE_MAX);
    char* str_buf = malloc(buf_len);

    errno = 0;
    int err = getpwuid_r(uid, &pwd_buf, str_buf, buf_len, &result);
    if (err == 0 && result != NULL) {
        name_len = strlen(result->pw_name);
        strcpy(*name_buf, result->pw_name);
    }

    free(str_buf);
    return name_len;
}

size_t get_group_name(gid_t gid, char* name_buf[]) {
    size_t name_len = 0;
    struct group result_buf;
    struct group* result = NULL;
    size_t buf_len = sysconf(_SC_GETGR_R_SIZE_MAX);
    char* byte_buf = malloc(buf_len * sizeof(char));

    errno = 0;
    int err = getgrgid_r(gid, &result_buf, byte_buf, buf_len, &result);
    if (err == 0 && result != NULL) {
        name_len = strlen(result->gr_name);
        strcpy(*name_buf, result->gr_name);
    }

    free(byte_buf);
    return name_len;
}

