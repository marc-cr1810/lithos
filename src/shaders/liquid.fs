#version 330 core

out vec4 FragColor;

in vec4 ourColor;
in vec2 TexCoord;
in vec3 Lighting; // x=Sky, y=Block, z=AO
in vec3 FragPos;
in vec3 FragWorldPos;
in vec2 FlowVector;
in float Fresnel;
in float WaterFlags;

uniform sampler2D texture1;
uniform float sunStrength;
uniform vec3 viewPos;
uniform vec3 u_SunPos; // Relative sun position for specular

void main()
{
    // Basic Texture Sample
    // Ideally use flow vector to distort UVs here
    vec4 texColor = texture(texture1, TexCoord);
    
    // Tint with Vertex Color (which carries the Climate Tint from Chunk)
    // currently ourColor comes from aColor in VS. 
    // We need to ensure Chunk passes the tint color into aColor.
    texColor *= ourColor;

    // Unpack Flags
    int flags = int(WaterFlags + 0.1);
    bool isLava = (flags & 1) != 0;

    // Lighting
    float sunLevel = Lighting.x * sunStrength;
    float blockLevel = Lighting.y;
    float lightVal = max(sunLevel, blockLevel);
    lightVal = max(0.2, lightVal); // Ambient min

    // Apply Lighting
    vec3 rgb = texColor.rgb * lightVal;
    
    // Apply Fresnel Alpha
    float alpha = texColor.a;
    
    if (!isLava) {
        // Modulate alpha by Fresnel term
        // Fresnel is high at glancing angles, low at straight-on
        // VS uses: rgba.a = clamp(0.8*fresnel, 0, 2);
        
        // We want: High Fresnel -> More Opaque
        // Low Fresnel -> More Transparent (but not invisible)
        
        alpha = clamp(alpha * Fresnel, 0.3, 0.95);
    } else {
        alpha = 1.0; // Lava is opaque
    }

    // Specular Highlight (Simple)
    if (!isLava) {
        vec3 norm = vec3(0.0, 1.0, 0.0);
        vec3 lightDir = normalize(u_SunPos);
        vec3 viewDir = normalize(viewPos - FragWorldPos);
        vec3 reflectDir = reflect(-lightDir, norm);
        
        float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);
        float specularStrength = 0.5 * sunStrength; // Only if sun is out
        
        rgb += vec3(1.0) * spec * specularStrength;
    }

    FragColor = vec4(rgb, alpha);
}
