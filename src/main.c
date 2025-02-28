// main.c
#include <stdio.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif

#include <libvpk.h>

#ifdef _WIN32
#define S_ISDIR(mode) (((mode) & _S_IFDIR) != 0)
#endif

#define MAX_PATH_LENGTH 260

// trim leading whitespace from a string
const char *trim_leading(const char *s) {
    while (*s && isspace((unsigned char)*s))
        s++;
    return s;
}

// check if path length is valid
int is_valid_path(const char *path) {
    return strlen(path) < MAX_PATH_LENGTH;
}

// check if a directory exists
int directory_exists(const char *path) {
    struct stat info;
    return (stat(path, &info) == 0 && S_ISDIR(info.st_mode));
}

// create a single directory; returns 0 on success
int create_directory(const char *dir_path) {
    if (directory_exists(dir_path)) {
        return 0; // directory already exists
    }
    #ifdef _WIN32
    if (_mkdir(dir_path) != 0) {
        if (errno == EEXIST)
            return 0;
        printf("Error creating directory: %s (errno: %d)\n", dir_path, errno);
        return 1;
    }
    #else
    if (mkdir(dir_path, 0755) != 0) {
        if (errno == EEXIST)
            return 0;
        printf("Error creating directory: %s\n", dir_path);
        return 1;
    }
    #endif
    return 0;
}

// recursively create nested directories; returns 0 on success
int create_nested_directories(const char *path) {
    char tmp[1024];
    char *p = NULL;
    size_t len;

    strncpy(tmp, path, sizeof(tmp));
    tmp[sizeof(tmp)-1] = '\0';
    len = strlen(tmp);
    if (len > 0 && (tmp[len-1] == '/' || tmp[len-1] == '\\'))
        tmp[len-1] = '\0';

    for (p = tmp + 1; *p; p++) {
        if (*p == '/' || *p == '\\') {
            *p = '\0';
            if (create_directory(tmp) != 0)
                return 1;
            *p = '/';
        }
    }
    if (create_directory(tmp) != 0)
        return 1;

    return 0;
}

int extract_vpk(VPKHandle handle, const char *output_dir, char *argv[]) {
    VPKFile file_iter = vpk_ffirst(handle);
    if (file_iter == NULL) {
        printf("no files found in the vpk.\n");
        return 1;
    }
    
    do {
        const char *orig_path = vpk_fpath(file_iter);
        // trim leading whitespace using isspace
        orig_path = trim_leading(orig_path);
        // if file path starts with a slash, skip it
        if (*orig_path == '/' || *orig_path == '\\')
            orig_path++;
        
        // skip if path is empty after trimming
        if (orig_path[0] == '\0') {
            printf("skipping empty file path.\n");
            file_iter = vpk_fnext(handle);
            continue;
        }
        
        char full_path[1024];
        snprintf(full_path, sizeof(full_path), "%s/%s", output_dir, orig_path);
        
        if (!is_valid_path(full_path)) {
            printf("path too long: %s\n", full_path);
            return 1;
        }
        
        char *last_slash = strrchr(full_path, '/');
        if (last_slash != NULL) {
            char dir_path[1024];
            size_t dir_len = last_slash - full_path;
            strncpy(dir_path, full_path, dir_len);
            dir_path[dir_len] = '\0';

            // checking
            const char *vpk_path = argv[1];
            const char *base = strrchr(vpk_path, '/');
            if (base == NULL)
                base = vpk_path;
            else
                base++;

            char vpk_name[1024];
            strncpy(vpk_name, base, sizeof(vpk_name));
            vpk_name[sizeof(vpk_name)-1] = '\0';

            char *ext = strrchr(vpk_name, '.');
            if (ext != NULL && strcmp(ext, ".vpk") == 0) {
                *ext = '\0';  // remove the ".vpk" part
            }

            char output_dir[1024];
            snprintf(output_dir, sizeof(output_dir), "%s_dir", vpk_name);

            printf("%s, %s\n", dir_path, output_dir);

            if (strcmp(dir_path, output_dir) == 0) {
                file_iter = vpk_fnext(handle);
                continue;
            }
            // end checking

            printf("creating nested directories: %s\n", dir_path);
            if (create_nested_directories(dir_path) != 0) {
                printf("failed to create nested directories: %s\n", dir_path);
                return 1;
            }
        }

        VPKFile file = vpk_fopen(handle, orig_path);
        if (file == NULL) {
            printf("Skipping non-file or failed to open: %s\n", orig_path);
            file_iter = vpk_fnext(handle);
            continue;
        }
        
        size_t file_size = vpk_flen(file);
        if (file_size == 0) {
            printf("Skipping empty file: %s\n", orig_path);
            vpk_fclose(file);
            file_iter = vpk_fnext(handle);
            continue;
        }
        
        char *file_data = vpk_malloc_and_read(file);
        if (file_data == NULL) {
            printf("Failed to allocate memory for file %s.\n", orig_path);
            vpk_fclose(file);
            return 1;
        }
        
        vpk_fread(file_data, 1, file_size, file);
        vpk_fclose(file);
        
        FILE *output_file = fopen(full_path, "wb");
        if (output_file == NULL) {
            perror("Failed to create file");
            return 1;
        }

        size_t written = fwrite(file_data, 1, file_size, output_file);
        if (written != file_size) {
            perror("Failed to write complete data to file");
            fclose(output_file);
            return 1;
        }

        if (fclose(output_file) != 0) {
            perror("Failed to close file");
            return 1;
        }
        
        printf("extracted file: %s\n", full_path);
        
        file_iter = vpk_fnext(handle);
    } while (file_iter != NULL);
    
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("usage: vpkextractor.exe <vpk_file>\n");
        return 1;
    }
    
    const char *vpk_path = argv[1];
    VPKHandle handle = vpk_load(vpk_path);
    if (handle == VPK_NULL_HANDLE) {
        printf("failed to open vpk file: %s\n", vpk_path);
        return 1;
    }
    
    const char *base = strrchr(vpk_path, '/');
    if (base == NULL)
        base = vpk_path;
    else
        base++;
    
    char vpk_name[1024];
    strncpy(vpk_name, base, sizeof(vpk_name));
    vpk_name[sizeof(vpk_name)-1] = '\0';

    size_t name_len = strlen(vpk_name);
    if (name_len > 4 && strcmp(vpk_name + name_len - 4, ".vpk") == 0)
        vpk_name[name_len - 4] = '\0';
    
    char output_dir[1024];
    snprintf(output_dir, sizeof(output_dir), "%s_dir", vpk_name);
    
    printf("creating output directory: %s\n", output_dir);
    if (create_nested_directories(output_dir) != 0) {
        printf("failed to create directory: %s\n", output_dir);
        vpk_close(handle);
        return 1;
    }
    
    if (extract_vpk(handle, output_dir, argv) != 0) {
        vpk_close(handle);
        return 1;
    }
    
    vpk_close(handle);
    printf("extraction complete.\n");
    return 0;
}