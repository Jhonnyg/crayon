#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <float.h>

#include "linmath.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define CRAYON_IMPLEMENTATION
#include "crayon.h"

#define MAX_SPHERES 16

inline float dot_vec3(const vec3 v1, const vec3 v2)
{
	return v1[0] * v2[0] + v1[1] * v2[1] + v1[2] * v2[2];
}

bool intersect_sphere(const vec3 sphere_pos, float sphere_radius,
	const vec3 ray_pos, const vec3 ray_dir, const float t_min, const float t_max,
	crayon_hit_info* info)
{
	vec3 oc;
	vec3_sub(oc, ray_pos, sphere_pos);
	float a = dot_vec3(ray_dir,ray_dir);
	float b = 2.0f * dot_vec3(oc, ray_dir);
	float c = dot_vec3(oc,oc) - sphere_radius * sphere_radius;
	float d = b * b - 4 * a * c;
	
	if (d > 0.0f)
	{
		vec3 tmp_p, tmp_n;
		float tmp = (-b - sqrt(b*b - a*c))/a;
		if (tmp < t_max && tmp > t_min)
		{
			vec3_scale(tmp_p, ray_dir, tmp);
			vec3_add(tmp_p, tmp_p, ray_pos);

			vec3_sub(info->normal, tmp_p, sphere_pos);
			vec3_scale(info->normal, info->normal, 1.0f / sphere_radius);

			info->t = tmp;

			return true;
		}
		tmp = (-b + sqrt(b*b - a*c))/a;
		if (tmp < t_max && tmp > t_min)
		{
			vec3_scale(info->position, ray_dir, tmp);
			vec3_add(info->position, info->position, ray_pos);

			vec3_sub(info->normal, info->position, sphere_pos);
			vec3_scale(info->normal, info->normal, 1.0f / sphere_radius);

			info->t = tmp;
			return true;
		}
	}

	return false;
}

struct scene_sphere
{
	vec3 p;
	float r;
};

struct scene_t
{
	scene_t()
	{
		memset(this, 0, sizeof(*this));
	}

	scene_sphere spheres[MAX_SPHERES];
	uint16_t     spheres_count; 
} scene;

bool intersect(const vec3 ray_pos, const vec3 ray_dir, crayon_hit_info* info)
{
	bool  did_hit = false;
	float t_min = 0.0f;
	float t_max = 999999.0f;

	crayon_hit_info info_tmp;

	for (uint16_t i=0; i < scene.spheres_count; i++)
	{
		if (intersect_sphere(scene.spheres[i].p, scene.spheres[i].r, ray_pos, ray_dir, t_min, t_max, &info_tmp))
		{
			t_max   = info_tmp.t;
			did_hit = true;
			*info   = info_tmp;
		}
	}

	return did_hit;
}

inline void color(crayon_hit_info* info, float* rgba_out)
{
	rgba_out[0] = info->normal[0] * 0.5f + 0.5f;
	rgba_out[1] = info->normal[1] * 0.5f + 0.5f;
	rgba_out[2] = info->normal[2] * 0.5f + 0.5f;
	rgba_out[3] = 1.0f;


	rgba_out[0] = 1.0f;
	rgba_out[1] = 1.0f;
	rgba_out[2] = 1.0f;
	rgba_out[3] = 1.0f;
}

void write_image(const char* out)
{
	uint16_t width  = crayon_context_width();
	uint16_t height = crayon_context_height();
	uint8_t* image  = (uint8_t*) malloc(width * height * 4);
	uint32_t pixel_ix = 0;

	for (uint16_t y=0; y < width; y++)
	{
		for (uint16_t x=0; x < height; x++)
		{
			float pixel[4];
			crayon_get_pixel(x,y,pixel);

			image[pixel_ix++] = pixel[0] * 255;
			image[pixel_ix++] = pixel[1] * 255;
			image[pixel_ix++] = pixel[2] * 255;
			image[pixel_ix++] = pixel[3] * 255;
		}
	}

	stbi_write_png(out, width, height, 4, (void*) image, 4 * width);

	free(image);
}

inline void make_sphere(float x, float y, float z, float r)
{
	scene.spheres[scene.spheres_count].p[0] = x;
	scene.spheres[scene.spheres_count].p[1] = y;
	scene.spheres[scene.spheres_count].p[2] = z;
	scene.spheres[scene.spheres_count].r    = r;
	scene.spheres_count++;
}

int main(int argc, char const *argv[])
{
	int width  = 512;
	int height = 512;
	int block  = 32;

	mat4x4 view, perspective, camera;

	mat4x4_identity(view);
	mat4x4_perspective(perspective, 45, ((float) width) / ((float) height), 0.1f, 100.0f);

	crayon_make_context(width,height,block);
	crayon_set_view(view);
	crayon_set_color_fn(color);
	crayon_set_intersect_fn(intersect);

	make_sphere(-2.0f, 0.5f, -10.0f, 1.5);
	make_sphere(2.0f, 1.0f, -5.0f, 1.0);
	make_sphere(1.0f, -1.0f, -10.0f, 2.0);

	while(crayon_has_next())
	{
		crayon_packet_t packet = crayon_get_next_packet();
		crayon_execute(packet);
	}

	write_image("./test.png");

	crayon_cleanup();

	return 0;
}