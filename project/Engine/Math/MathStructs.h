#pragma once

struct Vector2 {
	float x;
	float y;
};

struct Vector3 {
	float x;
	float y;
	float z;
};

struct Vector4 {
	float x;
	float y;
	float z;
	float w;
};

struct Matrix2x2 {
	float m[2][2];
};

struct Matrix3x3 {
	float m[3][3];
};

struct Matrix4x4 {
	float m[4][4];
};

struct Transform {
	Vector3 scale;
	Vector3 rotate;
	Vector3 translate;
};

struct Quaternion {
	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;
	float w = 1.0f;

	Quaternion() = default;
	Quaternion(float inX, float inY, float inZ, float inW)
		: x(inX), y(inY), z(inZ), w(inW) {}
};

struct QuaternionTransform {
	Vector3 scale{ 1.0f, 1.0f, 1.0f };
	Quaternion rotate{ 0.0f, 0.0f, 0.0f, 1.0f };
	Vector3 translate{ 0.0f, 0.0f, 0.0f };
};
