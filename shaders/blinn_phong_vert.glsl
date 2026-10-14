#version 330 core

layout (location = 0) in vec3 aPosition;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec3 aColor;

uniform mat4 uMVP;
uniform mat4 uModel;

out vec3 vWorldPosition;
out vec3 vWorldNormal;
out vec3 vColor;

void main() {
    vec4 worldPosition = uModel * vec4(aPosition, 1.0);
    vWorldPosition = worldPosition.xyz;
    vWorldNormal = mat3(transpose(inverse(uModel))) * aNormal;
    vColor = aColor;
    gl_Position = uMVP * vec4(aPosition, 1.0);
}
