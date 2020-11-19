// ENGGEN131 (2020) - Lab 9 (5th - 9th October, 2020)
// EXERCISE SIX - Da Vinci Code
//
// tinyray.c - A raytracer in exactly 400 lines of C 
// Author: Matthew Jakeman
//
// Entirely original code, inspired by the following resources:
//  - https://github.com/ssloy/tinyraytracer
//  - https://www.gabrielgambetta.com/computer-graphics-from-scratch/basic-ray-tracing.html
//  - https://www.scratchapixel.com/lessons/3d-basic-rendering/minimal-ray-tracer-rendering-simple-shapes/ray-sphere-intersection

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include <math.h>

// A single byte-type representing one channel of a pixel
typedef unsigned char byte;

/* Minimal Floating Point Vector Maths - Author: Matthew Jakeman */
typedef struct {
    float x;
    float y;
    float z;
} Vec3;

Vec3 vec3_new(float x, float y, float z) {
    Vec3 vec;
    vec.x = x;
    vec.y = y;
    vec.z = z;
    return vec;
}

Vec3 vec3_add(Vec3 a, Vec3 b) {
    return vec3_new(a.x + b.x, a.y + b.y, a.z + b.z);
}

Vec3 vec3_subtract(Vec3 a, Vec3 b) {
    return vec3_new(a.x - b.x, a.y - b.y, a.z - b.z);
}

Vec3 vec3_subtract_scalar(Vec3 a, float b) {
    return vec3_new(a.x - b, a.y - b, a.z - b);
}

Vec3 vec3_divide_scalar(Vec3 a, float b) {
    return vec3_new(a.x/b, a.y/b, a.z/b);
}

Vec3 vec3_multiply(Vec3 a, Vec3 b) {
    return vec3_new(a.x*b.x, a.y*b.y, a.z*b.z);
}

Vec3 vec3_multiply_scalar(Vec3 a, float b) {
    return vec3_new(a.x*b, a.y*b, a.z*b);
}

float vec3_dot(Vec3 a, Vec3 b) {
    return a.x*b.x + a.y*b.y + a.z*b.z;
}

float vec3_len(Vec3 v) {
    return (float)sqrtf((v.x * v.x) + (v.y * v.y) + (v.z * v.z));
}

Vec3 vec3_normalise(Vec3 v) {
    return vec3_divide_scalar(v, vec3_len(v)); // Calculate unit vector
}

Vec3 vec3_clamp(Vec3 v, float min_value, float max_value) {
    return vec3_new(
        fmin(fmax(v.x, min_value), max_value),
        fmin(fmax(v.y, min_value), max_value),
        fmin(fmax(v.z, min_value), max_value)
    );
}
// END - vectors

/* Struct Definitions */
typedef Vec3 RgbColour;

typedef struct {
    RgbColour diffuse;
    float specular;
} Material;

typedef struct {
    Vec3 centre;
    float radius;
    Material material;
} Sphere;

typedef struct {
    Vec3 origin;
    Vec3 direction;
} Ray;

typedef enum {
    PointLight,
    DirectionalLight,
    AmbientLight
} LightType;

typedef struct {
    LightType type;
    Vec3 position; // point only
    Vec3 direction; // directional only
    float intensity;
} Light;

/* Globals */
const static Vec3 Zero = {0};
const static Vec3 Invalid = {-1, -1, -1};
const static int FOV = 1; //3.1415/2;
const static int MAX_DIST = 1000;
const static unsigned int WIDTH = 600;
const static unsigned int HEIGHT = 600;

/* Constructors */
Sphere sphere_new(Vec3 centre, float radius, Material material) {
    Sphere sphere = {0};
    sphere.centre = centre;
    sphere.radius = radius;
    sphere.material = material;
    return sphere;
}

Light light_point_new(float intensity, Vec3 position) {
    Light light = {0};
    light.type = PointLight;
    light.position = position;
    light.intensity = intensity;
    return light;
}

Light light_ambient_new(float intensity) {
    Light light = {0};
    light.type = AmbientLight;
    light.intensity = intensity;
    return light;
}

Light light_directional_new(float intensity, Vec3 direction) {
    Light light = {0};
    light.type = DirectionalLight;
    light.direction = direction;
    light.intensity = intensity;
    return light;
}

Material material_new(Vec3 diffuse, float specular) {
    Material material = {0};
    material.diffuse = diffuse;
    material.specular = specular;
    return material;
}

Ray ray_new(Vec3 origin, Vec3 direction) {
    Ray ray;
    ray.origin = origin;
    ray.direction = direction;
    return ray;
}

// Lighting Compute Algorithm
// Adapted from https://www.gabrielgambetta.com/
//
// ARGS:
//  - point = point of intersection
//  - normal = normal vector at point
// RETURNS:
//  - intensity of light
float lighting_compute(Vec3 point, Vec3 normal,
                       Vec3 view, float specular,
                       Light *lights, int num_lights) {
    
    // Intensity of light for the given pixel
    float intensity = 0.0f;

    // Iterate over lights
    for (int i = 0; i < num_lights; i++)
    {
        Light *light = &lights[i];
        if (light->type == AmbientLight)
        {
            // Simply add ambient light to total
            intensity += light->intensity;
        }
        else
        {
            Vec3 light_ray;
            if (light->type == PointLight)
                // Point Light: Direction of ray from light to point
                light_ray = vec3_subtract(light->position, point);
            else
                // Directional Light: Direction
                light_ray = light->direction;		

            // Diffuse
            float reflect = vec3_dot(normal, light_ray);
            intensity += (light->intensity * reflect)/(vec3_len(normal) * vec3_len(light_ray));

            // Specular
            if (specular != -1)
            {
                Vec3 r = vec3_subtract(vec3_multiply_scalar(normal, 2 * vec3_dot(normal, light_ray)), light_ray);
                float reflect_view_proj = vec3_dot(r, view);
                if (reflect_view_proj > 0)
                {
                    float cosine = reflect_view_proj/(vec3_len(r) * vec3_len(view));
                    intensity += light->intensity * powf(cosine, specular);
                }
            }
        }
    }

    return intensity;
}

// Sphere-Ray Intersection
// Returns 1 if intersection found, otherwise 0
//
// ARGS:
//  - sphere = sphere to intersect
//  - ray = description of ray properties (e.g. direction)
// RETURNS:
//  - boolean of whether ray intersected a sphere
//  - [out] dist0, dist 1 = perpendicular distances to points of intersection
int do_sphere_raycast(Sphere sphere, Ray ray, float *dist0, float *dist1) {
    
    // Please see sphere_ray_intersection.bmp
    *dist0 = 0;
    *dist1 = 0;

    // Find L and tca
    Vec3 L = vec3_subtract(sphere.centre, ray.origin);
    float tca = vec3_dot(L, ray.direction);

    // Discard if intersection is behind origin
    if (tca < 0)
        return 0;

    // Find d
    float d = sqrtf(vec3_dot(L, L) - tca * tca);
    if (d > sphere.radius)
        return 0;

    // Calculate thc using pythagoras
    float thc = sqrtf(sphere.radius * sphere.radius - d * d);

    // Calculate t0 and t1 (perpendicular distance to
    // the 0th and 1st intersection)
    float t0 = tca - thc;
    float t1 = tca + thc;

    // Ensure at least one of t0 and t1 is greater than zero
    if (t0 < 0 && t1 < 0)
        return 0;

    *dist0 = t0;
    *dist1 = t1;
    
    return 1; // Intersection found
}

// Raytrace Scene at Point
//
// ARGS:
//  - origin = location of camera
//  - dir = direction of projectile
//  - min_t, max_t = min and max clipping planes
//  - spheres, num_spheres = array of spheres to test
//  - lights, num_lights = array of lights in the scene
// RETURNS:
//  - rgb colour of pixel being raytraced
RgbColour raytrace(Vec3 origin, Vec3 dir, float min_t, float max_t,
                   Sphere *spheres, int num_spheres,
                   Light *lights, int num_lights) {

    // Closest sphere to screen (for depth-testing)
    Sphere *closest = 0;
    
    // We use t_comp to store the t-depth of the closest
    // sphere and compare it with other spheres to perform
    // primitive depth testing (where 't' is perpendicular
    // distance to the point of intersection)
    float t_comp = (float)MAX_DIST;

    // Ray to test
    Ray ray = ray_new(origin, dir);

    // Cycle through all spheres and depth-test
    for (int i = 0; i < num_spheres; i++)
    {
        float dist0, dist1;
        if (do_sphere_raycast(spheres[i], ray, &dist0, &dist1))
        {
            // Check dist0
            if ((min_t < dist0 && dist0 < max_t) &&
                dist0 < t_comp)
            {
                t_comp = dist0;
                closest = &spheres[i];
            }

            // Now check dist1
            if ((min_t < dist1 && dist1 < max_t) &&
                dist1 < t_comp)
            {
                t_comp = dist1;
                closest = &spheres[i];
            }
        }
    }

    if (!closest)
        return Invalid;

    Vec3 point = vec3_add(origin, vec3_multiply_scalar(dir, t_comp));
    Vec3 normal = vec3_normalise(vec3_subtract(point, closest->centre));
    Material material = closest->material;

    return vec3_clamp(
        vec3_multiply_scalar(
            material.diffuse,
            lighting_compute(
                point, normal,
                vec3_multiply_scalar(dir, -1),
                material.specular,
                lights, num_lights
            )
        ),
        0.0f, 255.0f // Clamp between 0 and 255
    );
}

// Watermark: Says "MATT J"
#define MARK_COLS 33
#define MARK_ROWS 7
int mark[MARK_ROWS][MARK_COLS] = {
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 1, 1, 0, 1, 1, 0, 0, 1, 1, 1, 0, 0, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 0},
    {0, 1, 0, 1, 0, 1, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0},
    {0, 1, 0, 1, 0, 1, 0, 1, 1, 1, 1, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0},
    {0, 1, 0, 1, 0, 1, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0},
    {0, 1, 0, 1, 0, 1, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
};

// Draws a watermark on the screen using the above array
// NOTE: Make sure size and stride do not cause the function to exceed
// array bounds. This will cause a crash!
void DrawWatermark(byte *data) {
    int start_x = 20;
    int start_y = 540;
    int size = 3, stride = 1;

    for (int i = 0; i < MARK_COLS; i++) {
        for (int j = 0; j < MARK_ROWS; j++) {
            int y_corner = start_y + j*(size*2 + stride);
            int x_corner = start_x + i*(size*2 + stride);

            // Draw Square
            for (int x = x_corner; x < (x_corner + size); x++) {
                for (int y = y_corner; y < (y_corner + size); y++) {
                    if (mark[j][i] == 0) {
                        data[(y*WIDTH + x) * 3 + 0] = (byte)(i/(float)MARK_COLS * 255) % 180;
                        data[(y*WIDTH + x) * 3 + 1] = (byte)(j/(float)MARK_ROWS * 255) % 180;
                        data[(y*WIDTH + x) * 3 + 2] = (byte)240;
                    }
                    else {
                        data[(y*WIDTH + x) * 3 + 0] = (byte)255;
                        data[(y*WIDTH + x) * 3 + 1] = (byte)255;
                        data[(y*WIDTH + x) * 3 + 2] = (byte)255;
                    }
                }
            }
        }
    }
}

int main(void)
{
    byte *data = malloc(sizeof(byte) * 3 * WIDTH * HEIGHT);

    // Materials
    Material blue = material_new(vec3_new(69, 161, 255), 500);
    Material white = material_new(vec3_new(240, 240, 240), 180);
    Material red = material_new(vec3_new(255, 0, 57), 10);
    Material ground = material_new(vec3_new(0, 57, 89), 1000);

    // Scene
    #define NUM_SPHERES 4
    Sphere spheres[NUM_SPHERES];
    spheres[0] = sphere_new(vec3_new(-0.75f, -0.2f, 6.5f), 1.5f, red);
    spheres[1] = sphere_new(vec3_new(0, -1, 5), 1.0f, blue);
    spheres[2] = sphere_new(vec3_new(2, -0.5, 8), 3.0f, white);
    spheres[3] = sphere_new(vec3_new(0, -4001, 0), 4000, ground);

    // Lights
    #define NUM_LIGHTS 3
    Light lights[NUM_LIGHTS];
    lights[0] = light_ambient_new(0.2f);
    lights[1] = light_point_new(0.6f, vec3_new(-8, 1, 0));
    lights[2] = light_directional_new(0.2f, vec3_new(1, 4, -8));

    // For non-square images (future-proofing?)
    float aspect_ratio = (float)WIDTH/(float)HEIGHT;
    float screen_dim = tanf(FOV / (float)2);

    Vec3 origin = Zero;

    // Render
    for (int x = 0; x < WIDTH; x++) {
        for (int y = 0; y < HEIGHT; y++) {

            // Background
            data[(y*WIDTH + x) * 3 + 0] = (byte)(y/(float)WIDTH * 255);
            data[(y*WIDTH + x) * 3 + 1] = (byte)(x/(float)HEIGHT * 255);
            data[(y*WIDTH + x) * 3 + 2] = (byte)160;
            
            // Get Pixel in World Coords
            float x_world_coord = (2*(x + 0.5f)/(float)HEIGHT - 1) * screen_dim * aspect_ratio;
            float y_world_coord = -(2*(y + 0.5f)/(float)WIDTH - 1) * screen_dim;
            Vec3 dir = vec3_normalise(vec3_new(x_world_coord, y_world_coord, 1));

            // Raytrace Pixel
            RgbColour colour = raytrace(origin, dir, 1.0f, (float)MAX_DIST,
                                        spheres, NUM_SPHERES,
                                        lights, NUM_LIGHTS);

            // Draw Geometry
            if (colour.x != -1) {
                data[(y*WIDTH + x) * 3 + 0] = (byte)colour.x;
                data[(y*WIDTH + x) * 3 + 1] = (byte)colour.y;
                data[(y*WIDTH + x) * 3 + 2] = (byte)colour.z;
            }
        }
    }

    // Output
    DrawWatermark(data);

    // Write to file
    printf("tinyray: writing to file!");
    if (!stbi_write_bmp("output.bmp", WIDTH, HEIGHT, 3, data))
        printf("tinyray: failed to write image!");

    return 0;
}