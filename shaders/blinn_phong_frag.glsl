#version 330 core

in vec3 vWorldPosition;
in vec3 vWorldNormal;
in vec3 vColor;

uniform vec3 uEyePosition;
uniform vec3 uLightPosition;
uniform float uAmbientStrength;
uniform float uShininess;
uniform float uSpecularStrength;

out vec4 fragColor;

void main() {
    vec3 N = normalize(vWorldNormal);
    vec3 L = normalize(uLightPosition - vWorldPosition);
    vec3 V = normalize(uEyePosition - vWorldPosition);
    vec3 H = normalize(L + V);

    float diffuse = max(dot(N, L), 0.0);
    float specular = diffuse > 0.0 ? pow(max(dot(N, H), 0.0), uShininess) : 0.0;
    vec3 ambientTerm = uAmbientStrength * vColor;
    vec3 diffuseTerm = diffuse * vColor;
    vec3 specularTerm = uSpecularStrength * specular * vec3(1.0);
    fragColor = vec4(ambientTerm + diffuseTerm + specularTerm, 1.0);
}
