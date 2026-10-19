#pragma once
#include "Engine/Math/MathStructs.h"
#include <vector>

namespace Math {
	// ベクトルの加算を計算する関数
	Vector3 Add(const Vector3& v1, const Vector3& v2);
	// ベクトルの引き算を計算する関数
	Vector3 Subtract(const Vector3& v1, const Vector3& v2);
	// ベクトルをスカラー倍する関数
	Vector3 Multiply(float scalar, const Vector3& v);
	// 内積を計算する関数
	float Dot(const Vector3& v1, const Vector3& v2);
	// ベクトルを正規化する関数
	Vector3 Normalize(const Vector3& v);
	// ベクトルの長さを計算する関数
	float Length(const Vector3& v);
	// クロス積
	Vector3 Cross(const Vector3& v1, const Vector3& v2);
	// 座標変換(Matrix4x4からVector3へ)
	Vector3 Transform(const Matrix4x4& matrix, const Vector3& vector);
	// 正射影ベクトルを求める関数
	Vector3 Project(const Vector3& v1, Vector3& v2);
	// 最近接点を求める関数
	Vector3 ClosestPoint(const Vector3& lineStart, const Vector3& lineEnd, const Vector3& point);
	// ベクトルの長さを求める関数
	Vector3 Perpendicular(const Vector3& vector);
	// 指定されたVector3（ベクトル）を、与えられたMatrix4x4（行列）の回転成分のみを使って変換するための関数
	Vector3 TransformNormal(const Vector3& v, const Matrix4x4& m);

	// カメラの「ビュー行列（View Matrix）」を作るための関数
	Matrix4x4 MakeLookAtMatrix(const Vector3& eye, const Vector3& target, const Vector3& up);
	// 逆行列(Matrix4x4)
	Matrix4x4 Inverse(const Matrix4x4& m);
	// 平行移動行列(Matrix4x4)
	Matrix4x4 MakeTranslateMatrix(const Vector3& translate);
	// 拡大縮小行列(Matrix4x4)
	Matrix4x4 MakeScaleMatrix(const Vector3& scale);
	// 回転行列(Matrix4x4)
	// X軸回転行列
	Matrix4x4 MakeRotateXMatrix(float radian);
	// Y軸回転行列
	Matrix4x4 MakeRotateYMatrix(float radian);
	// Z軸回転行列
	Matrix4x4 MakeRotateZMatrix(float radian);
	// 回転行列
	Matrix4x4 MakeRotateMatrix(float roll, float pitch, float yaw);
	// アフィン変換行列(Matrix4x4)
	Matrix4x4 MakeAffineMatrix(const Vector3& scale, const Vector3& rotate, const Vector3& translate);
	// 投視投影行列
	Matrix4x4 MakePerspectiveFovMatrix(float fovY, float aspectRatio, float nearClip, float farClip);
	// 正射影行列
	Matrix4x4 MakeOrthographicMatrix(float left, float top, float right, float bottom, float nearClip, float farClip);
	// ビューポート変換行列
	Matrix4x4 MakeViewportMatrix(float left, float top, float width, float height, float minDepth, float maxDepth);
	// 行列の加算
	Matrix4x4 Add(const Matrix4x4& m1, const Matrix4x4& m2);
	// 行列の減算
	Matrix4x4 Subtract(const Matrix4x4& m1, const Matrix4x4& m2);
	// 行列の乗算
	Matrix4x4 Multiply(const Matrix4x4& a, const Matrix4x4& b);
	// 行列の転置
	Matrix4x4 Transpose(const Matrix4x4& m);
	// 単位ベクトルの作成
	Matrix4x4 MakeIdentity();
	// Catmull-Romスプラインによる補間座標を計算する関数
	Vector3 CatmullRomInterpolate(const Vector3& p0, const Vector3& p1, const Vector3& p2, const Vector3& p3, float t);
	// 制御点配列からCatmull-Romスプラインを用いて細分化された滑らかな曲線座標の配列を生成する関数
	std::vector<Vector3> GenerateCatmullRomPath(const std::vector<Vector3>& controlPoints, int division = 10);
}

// 二項演算子
Vector3 operator+(const Vector3& v1, const Vector3& v2);
Vector3 operator-(const Vector3& v1, const Vector3& v2);
Vector3 operator*(float s, const Vector3& v);
Vector3 operator*(const Vector3& v, float s);
Vector3 operator/(const Vector3& v, float s);
Matrix4x4 operator+(const Matrix4x4& m1, const Matrix4x4& m2);
Matrix4x4 operator-(const Matrix4x4& m1, const Matrix4x4& m2);
Matrix4x4 operator*(const Matrix4x4& m1, const Matrix4x4& m2);
// 単項演算子
Vector3 operator-(const Vector3& v);
Vector3 operator+(const Vector3& v);
// 複合代入演算子
Vector3& operator*=(Vector3& v, float s);
Vector3& operator-=(Vector3& v1, const Vector3& v2);
Vector3& operator+=(Vector3& v1, const Vector3& v2);
Vector3& operator/=(Vector3& v, float s);
