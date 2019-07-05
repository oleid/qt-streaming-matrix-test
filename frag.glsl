#version 330

in highp vec2 texCoord;
out highp vec4 f_color;

uniform sampler2D tex;
uniform int is_radar_plot;

const float PI = 3.1415926535897932384626433832795;

highp vec3 rainbow_table(int i)
{
#define C(r, g, b) r / 255., g / 255., b / 255.0
	const int N = 9;
	const float[N * 3] T = float[N * 3](C(21., 21., 33.),
			C(33., 47., 64.),
			C(37., 91., 84.),
			C(103., 81., 29.),
			C(104., 37., 90.),
			C(21., 71., 234.),
			C(0., 237., 2.),
			C(223., 224., 0.),
			C(255., 0., 0.));

#undef C
	i = (i >= N - 1) ? N - 1 : i;

	return highp vec3(T[i * 3 + 0], T[i * 3 + 1], T[i * 3 + 2]);
}

highp vec3 double_rainbow_rgb(float intensity)
{
	float which = 8.0f * clamp(intensity, 0.0f, 1.0f);

	float c1 = floor(which);
	float c2 = ceil(which);
	float s1 = 1. - abs(c1 - which);
	float s2 = 1. - abs(c2 - which);

	return rainbow_table(int(c1)) * s1 + rainbow_table(int(c2)) * s2;
}

void main()
{
	float RadiusMin = 0.0f;
	float RadiusMax = 1.0f;

	// Convert Cartesian to Polar coords
	highp vec2 normCoord = 2.0 * texCoord - highp vec2(1.0, 1.0);

	float intensity = 0.0f;

	if (is_radar_plot != 0)
	{
		float r = length(normCoord);
		float theta = (atan(normCoord.y, normCoord.x) / (2 * PI) + 0.5);

		normCoord = highp vec2(r, theta);
		intensity = float(r <= RadiusMax) * texture(tex, normCoord).x;
	}
	else
	{
		intensity = texture(tex, texCoord).x;
	}

	highp vec3 color = double_rainbow_rgb(intensity);
	f_color = highp vec4(color, 1.0);
}
