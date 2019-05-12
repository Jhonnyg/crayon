#ifndef CRAYON_H
#define CRAYON_H

/* CRAYON API
 * ==========
 */

static const uint16_t crayon_invalid_packet = 0xffff; 

struct crayon_hit_info
{
    vec3  position;
    vec3  normal;
    float t;
};

struct crayon_packet_t
{
    int32_t x;
    int32_t y;
    int32_t w;
    int32_t h;
};

struct crayon_free_list_entry_t;
struct crayon_free_list_entry_t
{
    uint32_t                  packet_index;
    crayon_free_list_entry_t* next;
};

struct crayon_free_list_t
{
    crayon_free_list_entry_t* head;
    crayon_free_list_entry_t* tail;
};

struct crayon_context_t
{
    crayon_context_t()
    {
        memset(this, 0, sizeof(*this));
    }

    mat4x4             camera_view;
    crayon_free_list_t packet_free_list;
    crayon_packet_t*   packet_list;
    float*             image_data;
    uint32_t           packet_count;
    uint16_t           image_width;
    uint16_t           image_height;

    bool (*intersect_fn)(const vec3, const vec3, crayon_hit_info*);
    void (*color_fn)(crayon_hit_info*, float*);
    void (*background_fn)(void);
} g_crayon_context;

static void            crayon_add_to_free(uint32_t packet_index);
static bool            crayon_make_context(uint16_t width, uint16_t height, uint16_t block);
static crayon_packet_t crayon_get_next_packet(void);
static void            crayon_execute(crayon_packet_t packet);
static void            crayon_cleanup(void);
static void            crayon_get_pixel(uint16_t x, uint16_t y, float* pixel);
static uint16_t        crayon_context_width();
static uint16_t        crayon_context_height();

#ifdef CRAYON_IMPLEMENTATION

static inline crayon_free_list_entry_t* crayon_make_free_entry(uint32_t packet_index)
{
    crayon_free_list_entry_t* new_entry = (crayon_free_list_entry_t*) malloc(sizeof(crayon_free_list_entry_t));
    new_entry->packet_index = packet_index;
    return new_entry;
}

static uint16_t crayon_context_width()
{
    return g_crayon_context.image_width;
}

static uint16_t crayon_context_height()
{
    return g_crayon_context.image_height;
}

static void crayon_get_pixel(uint16_t x, uint16_t y, float* pixel)
{
    uint32_t pixel_ix = (y * g_crayon_context.image_width + x) * 4;
    pixel[0] = g_crayon_context.image_data[pixel_ix + 0];
    pixel[1] = g_crayon_context.image_data[pixel_ix + 1];
    pixel[2] = g_crayon_context.image_data[pixel_ix + 2];
    pixel[3] = g_crayon_context.image_data[pixel_ix + 3];
}

static void crayon_add_to_free(uint32_t packet_index)
{
    crayon_free_list_entry_t* new_entry = crayon_make_free_entry(packet_index);
    crayon_free_list_entry_t* next_head = g_crayon_context.packet_free_list.head->next;
    g_crayon_context.packet_free_list.head->next = new_entry;
    new_entry->next = next_head;
}

static bool crayon_make_context(uint16_t width, uint16_t height, uint16_t block)
{
    assert(g_crayon_context.packet_list == 0);

    uint16_t num_packets_w   = (width + block-1) / block;
    uint16_t num_packets_h   = (height + block-1) / block;
    uint16_t num_packets     = num_packets_w * num_packets_h;
    size_t   image_data_size = sizeof(float) * width * height * 4;

    g_crayon_context.image_width  = width;
    g_crayon_context.image_height = height;
    g_crayon_context.image_data   = (float*) malloc(image_data_size);

    memset((void*)g_crayon_context.image_data, 0, image_data_size);

    g_crayon_context.packet_list  = (crayon_packet_t*) malloc(sizeof(crayon_packet_t) * num_packets);
    g_crayon_context.packet_count = num_packets;

    g_crayon_context.packet_free_list.head = crayon_make_free_entry(crayon_invalid_packet);
    g_crayon_context.packet_free_list.tail = crayon_make_free_entry(crayon_invalid_packet);

    g_crayon_context.packet_free_list.head->next = g_crayon_context.packet_free_list.tail; 
    g_crayon_context.packet_free_list.tail->next = NULL;

    for (uint16_t y=0; y < num_packets_h; y++)
    {
        for (uint16_t x=0; x < num_packets_w; x++)
        {
            uint32_t packet_index   = y * num_packets_w + x;
            crayon_packet_t* packet = &g_crayon_context.packet_list[packet_index];
            packet->x               = x * block;
            packet->y               = y * block;
            packet->w               = block;
            packet->h               = block;

            if ((packet->x + block) > width)
            {
                packet->w = width - packet->x;
            }

            if ((packet->y + block) > height)
            {
                packet->h = height - packet->y;
            }

            crayon_add_to_free(packet_index);
        }
    }

    return true;
}

static inline bool crayon_has_next()
{
    return g_crayon_context.packet_free_list.head->next != g_crayon_context.packet_free_list.tail;
}

static inline void crayon_set_view(mat4x4 view)
{
    mat4x4_dup(g_crayon_context.camera_view, view);
}

static inline void crayon_set_color_fn(void (*color)(crayon_hit_info*, float*))
{
    g_crayon_context.color_fn = color;
}

static inline void crayon_set_intersect_fn(bool (*intersect)(const vec3, const vec3, crayon_hit_info*))
{
    g_crayon_context.intersect_fn = intersect;
}

static crayon_packet_t crayon_get_next_packet()
{
    assert(g_crayon_context.packet_free_list.head->next != g_crayon_context.packet_free_list.tail);

    crayon_free_list_entry_t* next_head = g_crayon_context.packet_free_list.head->next;
    g_crayon_context.packet_free_list.head->next = next_head->next;
    
    uint32_t packet_index = next_head->packet_index;
    free(next_head);

    return g_crayon_context.packet_list[packet_index];
}

static void crayon_execute(crayon_packet_t packet)
{
    assert(g_crayon_context.intersect_fn);
    assert(g_crayon_context.color_fn);

    float iw          = (float) g_crayon_context.image_width;
    float ih          = (float) g_crayon_context.image_height;
    float inv_iw      = 1.0 / iw;
    float inv_ih      = 1.0 / ih;
    float fov         = 45.0;
    float aspectratio = iw / float(ih);  
    float angle       = tan(M_PI * 0.5 * fov / 180.0);

    const vec3 ray_pos = {0.0f, 0.0f, 0.0f};

    for (uint32_t y=0; y < packet.h; y++)
    {
        for (uint32_t x=0; x < packet.w; x++)
        {
            uint32_t image_x     = packet.x + x;
            uint32_t image_y     = packet.y + y;
            uint32_t image_start = (g_crayon_context.image_width * image_y + image_x) * 4;

            float fx             = (float) image_x;
            float fy             = (float) image_y;
            float u              = (2.0 * ((fx + 0.5) * inv_iw) - 1.0) * angle * aspectratio;
            float v              = (1.0 - 2.0 * ((fy + 0.5) * inv_ih)) * angle;

            float rgba[4] = {0.0f,0.0f,0.0f,1.0f};
            vec4 ray_dir  = {(float)u,(float)v,-1.0f,0.0f};
            vec4 ray_dir_out;
            crayon_hit_info hit;

            vec4_norm(ray_dir_out, ray_dir);
            mat4x4_mul_vec4(ray_dir_out, g_crayon_context.camera_view, ray_dir);

            if (g_crayon_context.intersect_fn(ray_pos, ray_dir_out, &hit))
            { 
                g_crayon_context.color_fn(&hit, rgba);
            }
            else if(g_crayon_context.background_fn)
            {
                g_crayon_context.background_fn();
            }

            g_crayon_context.image_data[image_start + 0] = rgba[0];
            g_crayon_context.image_data[image_start + 1] = rgba[1];
            g_crayon_context.image_data[image_start + 2] = rgba[2];
            g_crayon_context.image_data[image_start + 3] = rgba[3];
        }
    }
}

static void crayon_cleanup()
{
    free(g_crayon_context.packet_list);

    while(crayon_has_next())
    {
        crayon_get_next_packet();
    }

    free(g_crayon_context.packet_free_list.head);
    free(g_crayon_context.packet_free_list.tail);
    free(g_crayon_context.image_data);

    memset(&g_crayon_context, 0, sizeof(g_crayon_context));
}

#endif /* CRAYON_IMPLEMENTATION */

#endif /* CRAYON_H */
