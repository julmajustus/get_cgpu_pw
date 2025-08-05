/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   get_cgpu_pw.c                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: julmajustus <julmajustus@tutanota.com>     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/12/04 01:36:39 by julmajustus       #+#    #+#             */
/*   Updated: 2025/08/05 21:05:33 by julmajustus      ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <limits.h>
#include <string.h>

#define BUFFER_SIZE 128

static char *find_hwmon_file(const char *target, const char *filename)
{
	DIR *dir = opendir("/sys/class/hwmon");
	if (!dir)
	{
		perror("opendir");
		exit(EXIT_FAILURE);
	}

	static char full_path[PATH_MAX];
	struct dirent *entry;
	while ((entry = readdir(dir)) != NULL)
	{
		if (strncmp(entry->d_name, "hwmon", 5) != 0)
			continue;

		char name_path[PATH_MAX];
		snprintf(name_path, sizeof(name_path), "/sys/class/hwmon/%s/name", entry->d_name);

		FILE *f = fopen(name_path, "r");
		if (!f)
			continue;

		char name_buf[BUFFER_SIZE];
		if (fgets(name_buf, sizeof(name_buf), f) != NULL)
		{
			name_buf[strcspn(name_buf, "\n")] = '\0';
			if (strcmp(name_buf, target) == 0)
			{
				snprintf(full_path, sizeof(full_path), "/sys/class/hwmon/%s/%s", entry->d_name, filename);
				if (access(full_path, F_OK) == 0)
				{
					fclose(f);
					closedir(dir);
					return full_path;
				}
			}
		}
		fclose(f);
	}

	closedir(dir);
	return NULL;
}

static size_t get_cpu_energy(int fd)
{
	char buffer[BUFFER_SIZE];
	ssize_t bytes_read = pread(fd, buffer, BUFFER_SIZE - 1, 0);
	if (bytes_read <= 0)
	{
		perror("Failed to read file");
		exit(1);
	}

	buffer[bytes_read] = '\0';

	char *end;
	size_t energy = strtoull(buffer, &end, 10);
	if (end == buffer)
	{
		fprintf(stderr, "Failed to parse energy value\n");
		exit(1);
	}
	return (energy);
}

static double read_gpu_power(const char *gpu_path)
{
	int fd = open(gpu_path, O_RDONLY);
	if (fd == -1)
	{
		perror("Failed to open file");
		exit(EXIT_FAILURE);
	}

	char buffer[BUFFER_SIZE];
	ssize_t bytes_read = read(fd, buffer, BUFFER_SIZE - 1);
	if (bytes_read <= 0)
	{
		perror("Failed to read file");
		close(fd);
		exit(EXIT_FAILURE);
	}

	buffer[bytes_read] = '\0';
	close(fd);

	char *end;
	size_t power_micro_watts = strtol(buffer, &end, 10);
	if (end == buffer || power_micro_watts < 0)
	{
		fprintf(stderr, "Failed to parse power value\n");
		exit(EXIT_FAILURE);
	}

	return (power_micro_watts / 1000000.0);
}

int main(void)
{
	char *cpu_path = find_hwmon_file("zenergy", "energy17_input");

	if (!cpu_path)
	{
		fprintf(stderr, "CPU sensor file not found.\n");
		exit(EXIT_FAILURE);
	}

	int fd = open(cpu_path, O_RDONLY);

	if (fd == -1)
	{
		perror("Failed to open file");
		exit(1);
	}

	size_t initial_energy = get_cpu_energy(fd);
	usleep(10000);
	size_t final_energy = get_cpu_energy(fd);
	close(fd);
	size_t energy_diff = final_energy - initial_energy;
	double energy_diff_joules = energy_diff / 1000000.0;
	double power_usage = energy_diff_joules / 0.01;
	printf("%.1fW ", power_usage);

	char *gpu_path = find_hwmon_file("amdgpu", "power1_average");

	if (!gpu_path)
	{
		fprintf(stderr, "GPU sensor file not found.\n");
		exit(EXIT_FAILURE);
	}

	double power_watts = read_gpu_power(gpu_path);
	printf("%.1fW\n", power_watts);

	return 0;
}
