#version 100
precision highp float;
precision mediump int;

attribute vec3 gpu_Vertex;
attribute vec2 gpu_TexCoord;
attribute mediump vec4 gpu_Color;
uniform mat4 gpu_ModelViewProjectionMatrix;

varying mediump vec4 color;
varying /* PRAGMA: ONS_RU highprecision */ vec2 texCoord;

void main(void) {
	color = gpu_Color;
	texCoord = vec2(gpu_TexCoord);
	gl_Position = gpu_ModelViewProjectionMatrix * vec4(gpu_Vertex, 1.0);
}
