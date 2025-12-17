#version 330 core
out vec4 FragColor;

in vec3 ourColor;
in vec2 TexCoord;
in vec3 Lighting;

// texture sampler
uniform sampler2D texture1;
uniform bool useTexture;

void main()
{
    vec4 texColor = texture(texture1, TexCoord);
    if(!useTexture)
        texColor = vec4(1.0, 1.0, 1.0, 1.0); // Use white if no texture, so Vertex Color indicates color
    
    // Combine Texture * VertexColor * Lighting
    // Vertex Color is used for tinting (or if no texture is used, as the main color)
    // If we use texture, 'ourColor' might be white or specific tint.
    // For now, let's assume ourColor is used.
    
    FragColor = texColor * vec4(ourColor, 1.0) * vec4(Lighting, 1.0);
}
