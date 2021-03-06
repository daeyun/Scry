/**
 * @file trimesh_shape.glsl
 * @brief OpenGL shader based on the Blinn-Phong model.
 * @author Daeyun Shin <daeyun@dshin.org>
 * @version 0.1
 * @date 2015-01-02
 * @copyright librender is free software released under the BSD 2-Clause
 * license.
 */
#version 330 core
#define __SHADER_NAME__

uniform mat4 iModelViewMatrix;
uniform mat4 iProjectionMatrix;
uniform mat4 iModelViewProjectionMatrix;
uniform mat3 iVectorModelViewMatrix;

struct Light {
    bool IsEnabled;
    vec3 Color;
    vec3 Position;

    // Attenuation coefficients
    float ConstantAttenuation;
    float LinearAttenuation;
    float QuadraticAttenuation;
};

uniform vec3 iAmbient;
uniform int iNumLights;
uniform vec3 iEyeDirection;
uniform float iShininess;
uniform float iStrength;

uniform float iEdgeThickness;
uniform vec4 iEdgeColor;

const int NumMaxLights = 20;
uniform Light iLights[NumMaxLights];

#ifdef VERTEX_SHADER
// in view space
layout(location = 0) in vec3 VertexPosition;
layout(location = 1) in vec3 VertexNormal;
layout(location = 2) in vec4 VertexColor;
layout(location = 3) in vec2 VertexTexCoord;

out VS_GS_VERTEX {
    vec3 normal;
    vec4 color;
} vertex_out;

void main() {
    vertex_out.color = VertexColor;
    vertex_out.normal = VertexNormal;
    gl_Position = vec4(VertexPosition, 1);
}
#endif
#ifdef GEOMETRY_SHADER
layout(triangles) in;
layout(triangle_strip, max_vertices=3) out;

in VS_GS_VERTEX {
    vec3 normal;
    vec4 color;
} vertex_in[];

out GS_FS_VERTEX {
    vec4 position;
    vec4 color;
    vec3 normal;
    vec3 d;
} vertex_out;

void main() {
    vec4 mid[3];
    mid[0] = (gl_in[1].gl_Position+gl_in[2].gl_Position)/2.0;
    mid[1] = (gl_in[2].gl_Position+gl_in[0].gl_Position)/2.0;
    mid[2] = (gl_in[0].gl_Position+gl_in[1].gl_Position)/2.0;

    for (int i = 0; i < gl_in.length(); i++) {
        vertex_out.position = gl_in[i].gl_Position;
        vertex_out.normal = vertex_in[i].normal;
        vertex_out.color = vertex_in[i].color;

        vertex_out.d[(i+1)%3] = vertex_out.d[(i+2)%3] = 0;

        vec3 v1 = gl_in[i].gl_Position.xyz;
        vec3 v2 = gl_in[(i+1)%3].gl_Position.xyz;
        vec3 v3 = gl_in[(i+2)%3].gl_Position.xyz;

        vec3 n = normalize(v3-v2);
        vec3 a = v2;
        vec3 p = v1;

        vertex_out.d[i] = length((a-p)-dot(a-p, n)*n);

        gl_Position = iModelViewProjectionMatrix * vertex_out.position;
        EmitVertex();
    }
    EndPrimitive();
}
#endif
#ifdef FRAGMENT_SHADER

in GS_FS_VERTEX {
    vec4 position;
    vec4 color;
    vec3 normal;
    vec3 d;
} fragment_in;

layout(location=0) out vec4 FragmentColor;

void main() {
    vec3 normal = normalize(fragment_in.normal);
    vec4 col = vec4(iAmbient, fragment_in.color[3]);

    for (int i = 0; i < iNumLights; i++) {
        if (!iLights[i].IsEnabled) {
            continue;
        }

        vec3 light_dir = iLights[i].Position - vec3(fragment_in.position);
        float light_dist = length(light_dir);
        light_dir = light_dir / light_dist;

        float lambertian = max(dot(light_dir, fragment_in.normal), 0.0);
        float specular = 0.0;

        float attenuation = 1.0 /
            (iLights[i].ConstantAttenuation +
             (iLights[i].LinearAttenuation * light_dist) +
             (iLights[i].QuadraticAttenuation * light_dist * light_dist));

        if (lambertian > 0.0) {
            vec3 half_dir = normalize(light_dir + iEyeDirection);
            float spec_angle = max(dot(half_dir, fragment_in.normal), 0.0);
            specular = pow(spec_angle, iShininess) * iStrength;
        }

        col += vec4(lambertian * vec3(fragment_in.color) * attenuation +
               specular * mix(iLights[i].Color, vec3(fragment_in.color), 0.3)
               * attenuation, 0.0);
    }

    float edge_dist = min(min(fragment_in.d[0], fragment_in.d[1]),
            fragment_in.d[2]) / iEdgeThickness;

    // 4^(-d^2) = 0.00017
    if (edge_dist > 2.5 || iEdgeThickness < 1e-7) {
        FragmentColor = col;
        return;
    }

    float edge_intensity = pow(4, -pow(edge_dist, 2));
    FragmentColor = mix(col, iEdgeColor, edge_intensity);
}

#endif
