#version 330 core
out vec4 FragColor;

in vec3 ourColor;
in vec2 TexCoord;
in vec3 Lighting;

// texture sampler
uniform sampler2D texture1;
uniform bool useTexture;
uniform float sunStrength;

void main()
{
    vec4 texColor = texture(texture1, TexCoord);
    if(!useTexture)
        texColor = vec4(1.0, 1.0, 1.0, 1.0); // Use white if no texture, so Vertex Color indicates color
    
    // Lighting.x = SkyLight (0-1)
    // Lighting.y = BlockLight (0-1)
    
    // Phase 2: Multiply SkyLight by sunBrightness uniform
    float sunLevel = Lighting.x * sunStrength; 
    float blockLevel = Lighting.y;
    
    // Phase 4: AO
    float aoVal = Lighting.z; // 0, 1, 2, 3
    float aoFactor = 1.0 - (aoVal * 0.25); // 1.0, 0.75, 0.5, 0.25
    aoFactor = max(0.1, aoFactor); // Clamp
    
    float lightVal = max(sunLevel, blockLevel);
    lightVal = max(0.05, lightVal); // Ambient min

    // Apply AO to the final light multiplier or the color?
    // AO represents blocked ambient light.
    // It should darken everything.
    
    vec3 lightVec = vec3(lightVal * aoFactor);
    
    FragColor = texColor * vec4(ourColor, 1.0) * vec4(lightVec, 1.0);
}
