#include <cmath>
#include <assert.h>
#include "Engine/Math/Matrix/Matrix.h"

namespace Math {

	// ベクトルの加算を計算する関数
	Vector3 Add(const Vector3& v1, const Vector3& v2) {
		return { v1.x + v2.x, v1.y + v2.y, v1.z + v2.z };
	}

	// ベクトルの引き算を計算する関数
	Vector3 Subtract(const Vector3& v1, const Vector3& v2) {
		return { v1.x - v2.x, v1.y - v2.y, v1.z - v2.z };
	}

	// ベクトルをスカラー倍する関数
	Vector3 Multiply(float scalar, const Vector3& v) {
		return { scalar * v.x, scalar * v.y, scalar * v.z };
	}

	// 内積を計算する関数
	float Dot(const Vector3& v1, const Vector3& v2) {
		return v1.x * v2.x + v1.y * v2.y + v1.z * v2.z;
	}

	// ベクトルの長さを計算する関数
	float Length(const Vector3& v) {
		return sqrtf(Dot(v, v));
	}

	// ベクトルを正規化する関数
	Vector3 Normalize(const Vector3& v) {
		float length = Length(v);
		if (length == 0) {
			return { 0, 0, 0 };
		}
		return Multiply(1.0f / length, v);
	}

	//クロス積
	Vector3 Cross(const Vector3& v1, const Vector3& v2) {
		Vector3 result = { v1.y * v2.z - v1.z * v2.y,v1.z * v2.x - v1.x * v2.z,v1.x * v2.y - v1.y * v2.x };
		return result;
	}

	// 座標変換(Matrix4x4からVector3へ)
	Vector3 Transform(const Matrix4x4& matrix, const Vector3& vector) {
		Vector3 result = {};
		result.x = matrix.m[0][0] * vector.x + matrix.m[1][0] * vector.y + matrix.m[2][0] * vector.z + matrix.m[3][0];
		result.y = matrix.m[0][1] * vector.x + matrix.m[1][1] * vector.y + matrix.m[2][1] * vector.z + matrix.m[3][1];
		result.z = matrix.m[0][2] * vector.x + matrix.m[1][2] * vector.y + matrix.m[2][2] * vector.z + matrix.m[3][2];
		float w = matrix.m[0][3] * vector.x + matrix.m[1][3] * vector.y + matrix.m[2][3] * vector.z + matrix.m[3][3];
		assert(w != 0.0f);
		result.x /= w;
		result.y /= w;
		result.z /= w;
		return result;
	}

	// 正射影ベクトルを求める関数
	Vector3 Project(const Vector3& v1, Vector3& v2) {
		Vector3 result = {};
		float dot = v1.x * v2.x + v1.y * v2.y + v1.z * v2.z;
		float length = Length(v2);
		if (length != 0) {
			result.x = (dot / static_cast<float>(pow(length, 2))) * v2.x;
			result.y = (dot / static_cast<float>(pow(length, 2))) * v2.y;
			result.z = (dot / static_cast<float>(pow(length, 2))) * v2.z;
		}
		return result;
	}

	// 最近接点を求める関数
	Vector3 ClosestPoint(const Vector3& lineStart, const Vector3& lineEnd, const Vector3& point) {
		Vector3 lineDir = { lineEnd.x - lineStart.x, lineEnd.y - lineStart.y, lineEnd.z - lineStart.z };
		Vector3 pointToLineStart = { point.x - lineStart.x, point.y - lineStart.y, point.z - lineStart.z };
		float t = (lineDir.x * pointToLineStart.x + lineDir.y * pointToLineStart.y + lineDir.z * pointToLineStart.z) /
			(lineDir.x * lineDir.x + lineDir.y * lineDir.y + lineDir.z * lineDir.z);
		if (t < 0) {
			return { lineStart.x, lineStart.y, lineStart.z };
		} else if (t > 1) {
			return { lineEnd.x, lineEnd.y, lineEnd.z };
		} else {
			return { lineStart.x + t * lineDir.x, lineStart.y + t * lineDir.y, lineStart.z + t * lineDir.z };
		}
	}

	// ベクトルの長さを求める関数
	Vector3 Perpendicular(const Vector3& vector) {
		if (vector.x != 0.0f || vector.y != 0.0f) {
			return { -vector.y, vector.x, 0.0f };
		}
		return { 0.0f, -vector.z, vector.y };
	}
	// 指定されたVector3（ベクトル）を、与えられたMatrix4x4（行列）の回転成分のみを使って変換するための関数
	Vector3 TransformNormal(const Vector3& v, const Matrix4x4& m) {
		Vector3 result;
		result.x = v.x * m.m[0][0] + v.y * m.m[1][0] + v.z * m.m[2][0];
		result.y = v.x * m.m[0][1] + v.y * m.m[1][1] + v.z * m.m[2][1];
		result.z = v.x * m.m[0][2] + v.y * m.m[1][2] + v.z * m.m[2][2];
		return result;
	}

	// カメラの「ビュー行列（View Matrix）」を作るための関数
	Matrix4x4 MakeLookAtMatrix(const Vector3& eye, const Vector3& target, const Vector3& up) {
		Vector3 z = Normalize(Subtract(target,eye));
		Vector3 x = Normalize(Cross(up, z));
		Vector3 y = Cross(z, x);

		Matrix4x4 m = {};
		m.m[0][0] = x.x; m.m[0][1] = y.x; m.m[0][2] = z.x; m.m[0][3] = 0.0f;
		m.m[1][0] = x.y; m.m[1][1] = y.y; m.m[1][2] = z.y; m.m[1][3] = 0.0f;
		m.m[2][0] = x.z; m.m[2][1] = y.z; m.m[2][2] = z.z; m.m[2][3] = 0.0f;
		m.m[3][0] = -Dot(x, eye);
		m.m[3][1] = -Dot(y, eye);
		m.m[3][2] = -Dot(z, eye);
		m.m[3][3] = 1.0f;
		return m;
	}

	// 逆行列(Matrix4x4)
	Matrix4x4 Inverse(const Matrix4x4& m) {
		Matrix4x4 result = { 0 };
		Matrix4x4 num = { 0 };
		float det =
			m.m[0][0] * m.m[1][1] * m.m[2][2] * m.m[3][3]//1
			+ m.m[0][0] * m.m[1][2] * m.m[2][3] * m.m[3][1]//2
			+ m.m[0][0] * m.m[1][3] * m.m[2][1] * m.m[3][2]//3
			- m.m[0][0] * m.m[1][3] * m.m[2][2] * m.m[3][1]//4
			- m.m[0][0] * m.m[1][2] * m.m[2][1] * m.m[3][3]//5
			- m.m[0][0] * m.m[1][1] * m.m[2][3] * m.m[3][2]//6
			- m.m[0][1] * m.m[1][0] * m.m[2][2] * m.m[3][3]//7
			- m.m[0][2] * m.m[1][0] * m.m[2][3] * m.m[3][1]//8
			- m.m[0][3] * m.m[1][0] * m.m[2][1] * m.m[3][2]//9
			+ m.m[0][3] * m.m[1][0] * m.m[2][2] * m.m[3][1]//10
			+ m.m[0][2] * m.m[1][0] * m.m[2][1] * m.m[3][3]//11
			+ m.m[0][1] * m.m[1][0] * m.m[2][3] * m.m[3][2]//12
			+ m.m[0][1] * m.m[1][2] * m.m[2][0] * m.m[3][3]//13
			+ m.m[0][2] * m.m[1][3] * m.m[2][0] * m.m[3][1]//14
			+ m.m[0][3] * m.m[1][1] * m.m[2][0] * m.m[3][2]//15
			- m.m[0][3] * m.m[1][2] * m.m[2][0] * m.m[3][1]//16
			- m.m[0][2] * m.m[1][1] * m.m[2][0] * m.m[3][3]//17
			- m.m[0][1] * m.m[1][3] * m.m[2][0] * m.m[3][2]//18
			- m.m[0][1] * m.m[1][2] * m.m[2][3] * m.m[3][0]//19
			- m.m[0][2] * m.m[1][3] * m.m[2][1] * m.m[3][0]//20
			- m.m[0][3] * m.m[1][1] * m.m[2][2] * m.m[3][0]//21
			+ m.m[0][3] * m.m[1][2] * m.m[2][1] * m.m[3][0]//22
			+ m.m[0][2] * m.m[1][1] * m.m[2][3] * m.m[3][0]//23
			+ m.m[0][1] * m.m[1][3] * m.m[2][2] * m.m[3][0];//24
		// 行列式が0の場合、逆行列は存在しない
		// 適切なエラーハンドリングを行う
		if (det == 0) {
			return result; // ここでは単純にゼロ行列を返す
		}

		// 逆行列の計算
		num.m[0][0] =
			m.m[1][1] * m.m[2][2] * m.m[3][3]//1
			+ m.m[1][2] * m.m[2][3] * m.m[3][1]//2
			+ m.m[1][3] * m.m[2][1] * m.m[3][2]//3
			- m.m[1][3] * m.m[2][2] * m.m[3][1]//4
			- m.m[1][2] * m.m[2][1] * m.m[3][3]//5
			- m.m[1][1] * m.m[2][3] * m.m[3][2];//6
		num.m[0][1] =
			-m.m[0][1] * m.m[2][2] * m.m[3][3]//1
			- m.m[0][2] * m.m[2][3] * m.m[3][1]//2
			- m.m[0][3] * m.m[2][1] * m.m[3][2]//3
			+ m.m[0][3] * m.m[2][2] * m.m[3][1]//4
			+ m.m[0][2] * m.m[2][1] * m.m[3][3]//5
			+ m.m[0][1] * m.m[2][3] * m.m[3][2];//6
		num.m[0][2] =
			m.m[0][1] * m.m[1][2] * m.m[3][3]//1
			+ m.m[0][2] * m.m[1][3] * m.m[3][1]//2
			+ m.m[0][3] * m.m[1][1] * m.m[3][2]//3
			- m.m[0][3] * m.m[1][2] * m.m[3][1]//4
			- m.m[0][2] * m.m[1][1] * m.m[3][3]//5
			- m.m[0][1] * m.m[1][3] * m.m[3][2];//6
		num.m[0][3] =
			-m.m[0][1] * m.m[1][2] * m.m[2][3]//1
			- m.m[0][2] * m.m[1][3] * m.m[2][1]//2
			- m.m[0][3] * m.m[1][1] * m.m[2][2]//3
			+ m.m[0][3] * m.m[1][2] * m.m[2][1]//4
			+ m.m[0][2] * m.m[1][1] * m.m[2][3]//5
			+ m.m[0][1] * m.m[1][3] * m.m[2][2];//6

		num.m[1][0] =
			-m.m[1][0] * m.m[2][2] * m.m[3][3]//1
			- m.m[1][2] * m.m[2][3] * m.m[3][0]//2
			- m.m[1][3] * m.m[2][0] * m.m[3][2]//3
			+ m.m[1][3] * m.m[2][2] * m.m[3][0]//4
			+ m.m[1][2] * m.m[2][0] * m.m[3][3]//5
			+ m.m[1][0] * m.m[2][3] * m.m[3][2];//6
		num.m[1][1] =
			m.m[0][0] * m.m[2][2] * m.m[3][3]//1
			+ m.m[0][2] * m.m[2][3] * m.m[3][0]//2
			+ m.m[0][3] * m.m[2][0] * m.m[3][2]//3
			- m.m[0][3] * m.m[2][2] * m.m[3][0]//4
			- m.m[0][2] * m.m[2][0] * m.m[3][3]//5
			- m.m[0][0] * m.m[2][3] * m.m[3][2];//6
		num.m[1][2] =
			-m.m[0][0] * m.m[1][2] * m.m[3][3]//1
			- m.m[0][2] * m.m[1][3] * m.m[3][0]//2
			- m.m[0][3] * m.m[1][0] * m.m[3][2]//3
			+ m.m[0][3] * m.m[1][2] * m.m[3][0]//4
			+ m.m[0][2] * m.m[1][0] * m.m[3][3]//5
			+ m.m[0][0] * m.m[1][3] * m.m[3][2];//6
		num.m[1][3] =
			m.m[0][0] * m.m[1][2] * m.m[2][3]//1
			+ m.m[0][2] * m.m[1][3] * m.m[2][0]//2
			+ m.m[0][3] * m.m[1][0] * m.m[2][2]//3
			- m.m[0][3] * m.m[1][2] * m.m[2][0]//4
			- m.m[0][2] * m.m[1][0] * m.m[2][3]//5
			- m.m[0][0] * m.m[1][3] * m.m[2][2];//6

		num.m[2][0] =
			m.m[1][0] * m.m[2][1] * m.m[3][3]//1
			+ m.m[1][1] * m.m[2][3] * m.m[3][0]//2
			+ m.m[1][3] * m.m[2][0] * m.m[3][1]//3
			- m.m[1][3] * m.m[2][1] * m.m[3][0]//4
			- m.m[1][1] * m.m[2][0] * m.m[3][3]//5
			- m.m[1][0] * m.m[2][3] * m.m[3][1];//6
		num.m[2][1] =
			-m.m[0][0] * m.m[2][1] * m.m[3][3]//1
			- m.m[0][1] * m.m[2][3] * m.m[3][0]//2
			- m.m[0][3] * m.m[2][0] * m.m[3][1]//3
			+ m.m[0][3] * m.m[2][1] * m.m[3][0]//4
			+ m.m[0][1] * m.m[2][0] * m.m[3][3]//5
			+ m.m[0][0] * m.m[2][3] * m.m[3][1];//6
		num.m[2][2] =
			m.m[0][0] * m.m[1][1] * m.m[3][3]//1
			+ m.m[0][1] * m.m[1][3] * m.m[3][0]//2
			+ m.m[0][3] * m.m[1][0] * m.m[3][1]//3
			- m.m[0][3] * m.m[1][1] * m.m[3][0]//4
			- m.m[0][1] * m.m[1][0] * m.m[3][3]//5
			- m.m[0][0] * m.m[1][3] * m.m[3][1];//6
		num.m[2][3] =
			-m.m[0][0] * m.m[1][1] * m.m[2][3]//1
			- m.m[0][1] * m.m[1][3] * m.m[2][0]//2
			- m.m[0][3] * m.m[1][0] * m.m[2][1]//3
			+ m.m[0][3] * m.m[1][1] * m.m[2][0]//4
			+ m.m[0][1] * m.m[1][0] * m.m[2][3]//5
			+ m.m[0][0] * m.m[1][3] * m.m[2][1];//6

		num.m[3][0] =
			-m.m[1][0] * m.m[2][1] * m.m[3][2]//1
			- m.m[1][1] * m.m[2][2] * m.m[3][0]//2
			- m.m[1][2] * m.m[2][0] * m.m[3][1]//3
			+ m.m[1][2] * m.m[2][1] * m.m[3][0]//4
			+ m.m[1][1] * m.m[2][0] * m.m[3][2]//5
			+ m.m[1][0] * m.m[2][2] * m.m[3][1];//6
		num.m[3][1] =
			m.m[0][0] * m.m[2][1] * m.m[3][2]//1
			+ m.m[0][1] * m.m[2][2] * m.m[3][0]//2
			+ m.m[0][2] * m.m[2][0] * m.m[3][1]//3
			- m.m[0][2] * m.m[2][1] * m.m[3][0]//4
			- m.m[0][1] * m.m[2][0] * m.m[3][2]//5
			- m.m[0][0] * m.m[2][2] * m.m[3][1];//6
		num.m[3][2] =
			-m.m[0][0] * m.m[1][1] * m.m[3][2]//1
			- m.m[0][1] * m.m[1][2] * m.m[3][0]//2
			- m.m[0][2] * m.m[1][0] * m.m[3][1]//3
			+ m.m[0][2] * m.m[1][1] * m.m[3][0]//4
			+ m.m[0][1] * m.m[1][0] * m.m[3][2]//5
			+ m.m[0][0] * m.m[1][2] * m.m[3][1];//6
		num.m[3][3] =
			m.m[0][0] * m.m[1][1] * m.m[2][2]//1
			+ m.m[0][1] * m.m[1][2] * m.m[2][0]//2
			+ m.m[0][2] * m.m[1][0] * m.m[2][1]//3
			- m.m[0][2] * m.m[1][1] * m.m[2][0]//4
			- m.m[0][1] * m.m[1][0] * m.m[2][2]//5
			- m.m[0][0] * m.m[1][2] * m.m[2][1];//6
		// 行列式で割る
		for (int i = 0; i < 4; ++i) {
			for (int j = 0; j < 4; ++j) {
				result.m[i][j] = num.m[i][j] / det;
			}
		}
		return result;
	}

	// 平行移動行列(Matrix4x4)
	Matrix4x4 MakeTranslateMatrix(const Vector3& translate) {
		Matrix4x4 matrix = {};
		matrix.m[0][0] = 1.0f;
		matrix.m[1][1] = 1.0f;
		matrix.m[2][2] = 1.0f;
		matrix.m[3][3] = 1.0f;
		matrix.m[3][0] = translate.x;
		matrix.m[3][1] = translate.y;
		matrix.m[3][2] = translate.z;
		return matrix;
	}

	// 拡大縮小行列(Matrix4x4)
	Matrix4x4 MakeScaleMatrix(const Vector3& scale) {
		Matrix4x4 matrix = {};
		matrix.m[0][0] = scale.x;
		matrix.m[1][1] = scale.y;
		matrix.m[2][2] = scale.z;
		matrix.m[3][3] = 1.0f;
		return matrix;
	}

	// 回転行列(Matrix4x4)
	// X軸回転行列
	Matrix4x4  MakeRotateXMatrix(float radian) {
		Matrix4x4 matrix = {};
		matrix.m[0][0] = 1;
		matrix.m[1][1] = std::cosf(radian);
		matrix.m[1][2] = std::sinf(radian);
		matrix.m[2][1] = -std::sinf(radian);
		matrix.m[2][2] = std::cosf(radian);
		matrix.m[3][3] = 1;
		return matrix;
	}

	// Y軸回転行列
	Matrix4x4  MakeRotateYMatrix(float radian) {
		Matrix4x4 matrix = {};
		matrix.m[0][0] = std::cosf(radian);
		matrix.m[0][2] = -std::sinf(radian);
		matrix.m[1][1] = 1;
		matrix.m[2][0] = std::sinf(radian);
		matrix.m[2][2] = std::cosf(radian);
		matrix.m[3][3] = 1;
		return matrix;
	}

	// Z軸回転行列
	Matrix4x4  MakeRotateZMatrix(float radian) {
		Matrix4x4 matrix = {};
		matrix.m[0][0] = std::cosf(radian);
		matrix.m[0][1] = std::sinf(radian);
		matrix.m[1][0] = -std::sinf(radian);
		matrix.m[1][1] = std::cosf(radian);
		matrix.m[2][2] = 1;
		matrix.m[3][3] = 1;
		return matrix;
	}

	// 回転行列
	Matrix4x4 MakeRotateMatrix(float roll, float pitch, float yaw) {
		Matrix4x4 rotateX = MakeRotateXMatrix(roll);
		Matrix4x4 rotateY = MakeRotateYMatrix(pitch);
		Matrix4x4 rotateZ = MakeRotateZMatrix(yaw);
		return Multiply(Multiply(rotateX, rotateY), rotateZ);
	}

	//　アフィン変換行列(Matrix4x4)
	Matrix4x4 MakeAffineMatrix(const Vector3& scale, const Vector3& rotate, const Vector3& translate) {
		// 拡大縮小行列、回転行列、平行移動行列を作成
		Matrix4x4 scaleMatrix = MakeScaleMatrix(scale);
		Matrix4x4 rotateMatrix = MakeRotateMatrix(rotate.x, rotate.y, rotate.z);
		Matrix4x4 translateMatrix = MakeTranslateMatrix(translate);
		// 拡大縮小、回転、平行移動の順で行列を乗算(W=SRT:[W:WorldMatrix][S=ScaleMatrix][R=RotateMatrix][T=TranslateMatrix])
		return Multiply(Multiply(scaleMatrix, rotateMatrix), translateMatrix);
	}

	// 透視投影行列
	Matrix4x4 MakePerspectiveFovMatrix(float fovY, float aspectRatio, float nearClip, float farClip) {
		Matrix4x4 result = {};
		float f = 1.0f / std::tanf(fovY / 2.0f);
		result.m[0][0] = f / aspectRatio;
		result.m[1][1] = f;
		result.m[2][2] = farClip / (farClip - nearClip);
		result.m[2][3] = 1.0f;
		result.m[3][2] = (-nearClip * farClip) / (farClip - nearClip);
		return result;
	}

	// 正射影行列
	Matrix4x4 MakeOrthographicMatrix(float left, float top, float right, float bottom, float nearClip, float farClip) {
		Matrix4x4 result = {};
		result.m[0][0] = 2.0f / (right - left);
		result.m[1][1] = 2.0f / (top - bottom);
		result.m[2][2] = 1.0f / (farClip - nearClip);
		result.m[3][0] = (left + right) / (left - right);
		result.m[3][1] = (top + bottom) / (bottom - top);
		result.m[3][2] = nearClip / (nearClip - farClip);
		result.m[3][3] = 1.0f;
		return result;
	}

	// ビューポート変換行列	
	Matrix4x4 MakeViewportMatrix(float left, float top, float width, float height, float minDepth, float maxDepth) {
		Matrix4x4 result = {};
		result.m[0][0] = width / 2.0f;
		result.m[1][1] = -height / 2.0f;
		result.m[2][2] = maxDepth - minDepth;
		result.m[3][0] = left + width / 2.0f;
		result.m[3][1] = top + height / 2.0f;
		result.m[3][2] = minDepth;
		result.m[3][3] = 1.0f;
		return result;
	}

	// 行列の加算
	Matrix4x4 Add(const Matrix4x4& m1, const Matrix4x4& m2) {
		Matrix4x4 result;
		for (int i = 0; i < 4; ++i) {
			for (int j = 0; j < 4; ++j) {
				result.m[i][j] = m1.m[i][j] + m2.m[i][j];
			}
		}
		return result;
	}

	// 行列の減算
	Matrix4x4 Subtract(const Matrix4x4& m1, const Matrix4x4& m2) {
		Matrix4x4 result;
		for (int i = 0; i < 4; ++i) {
			for (int j = 0; j < 4; ++j) {
				result.m[i][j] = m1.m[i][j] - m2.m[i][j];
			}
		}
		return result;
	}

	// 行列の乗算
	Matrix4x4 Multiply(const Matrix4x4& a, const Matrix4x4& b) {
		Matrix4x4 result = {};
		for (int i = 0; i < 4; ++i) {
			for (int j = 0; j < 4; ++j) {
				result.m[i][j] = a.m[i][0] * b.m[0][j] + a.m[i][1] * b.m[1][j] +
					a.m[i][2] * b.m[2][j] + a.m[i][3] * b.m[3][j];
			}
		}
		return result;
	}

	// 行列の転置
	Matrix4x4 Transpose(const Matrix4x4& m) {
		Matrix4x4 result = { 0 };
		for (int i = 0; i < 4; ++i) {
			for (int j = 0; j < 4; ++j) {
				result.m[i][j] = m.m[j][i];
			}
		}
		return result;
	}

	// 単位行列の作成
	Matrix4x4 MakeIdentity() {
		Matrix4x4 identity = { 0 };
		for (int i = 0; i < 4; ++i) {
			identity.m[i][i] = 1.0f;
		}
		return identity;
	}

	// Catmull-Romスプラインによる補間座標を計算する関数
	Vector3 CatmullRomInterpolate(const Vector3& p0, const Vector3& p1, const Vector3& p2, const Vector3& p3, float t) {
		const float kHalf = 0.5f;
		const float kTwo = 2.0f;
		const float kThree = 3.0f;
		const float kFour = 4.0f;
		const float kFive = 5.0f;

		float t2 = t * t;
		float t3 = t2 * t;

		Vector3 a = kTwo * p1;
		Vector3 b = Subtract(p2, p0);
		Vector3 c = Subtract(Add(kTwo * p0, kFour * p2), Add(kFive * p1, p3));
		Vector3 d = Subtract(Add(Subtract(p3, p0), kThree * p1), kThree * p2);

		Vector3 result = kHalf * Add(Add(a, Multiply(t, b)), Add(Multiply(t2, c), Multiply(t3, d)));
		return result;
	}

	// 制御点配列からCatmull-Romスプラインを用いて細分化された滑らかな曲線座標の配列を生成する関数
	std::vector<Vector3> GenerateCatmullRomPath(const std::vector<Vector3>& controlPoints, int division) {
		std::vector<Vector3> path;
		if (controlPoints.size() < 4) {
			return controlPoints;
		}

		const int kMinDivision = 1;
		int div = division < kMinDivision ? kMinDivision : division;
		float step = 1.0f / static_cast<float>(div);

		size_t n = controlPoints.size();
		
		for (size_t i = 0; i < n - 1; ++i) {
			Vector3 p0 = (i == 0) ? Subtract(controlPoints[0], Subtract(controlPoints[1], controlPoints[0])) : controlPoints[i - 1];
			Vector3 p1 = controlPoints[i];
			Vector3 p2 = controlPoints[i + 1];
			Vector3 p3 = (i == n - 2) ? Add(controlPoints[n - 1], Subtract(controlPoints[n - 1], controlPoints[n - 2])) : controlPoints[i + 2];

			for (int j = 0; j < div; ++j) {
				float t = static_cast<float>(j) * step;
				path.push_back(CatmullRomInterpolate(p0, p1, p2, p3, t));
			}
		}

		path.push_back(controlPoints.back());
		return path;
	}
}

// 二公演算子
Vector3 operator+(const Vector3& v1, const Vector3& v2) {
	return Math::Add(v1, v2);
}
Vector3 operator-(const Vector3& v1, const Vector3& v2) {
	return Math::Subtract(v1, v2);
}
Vector3 operator*(float s, const Vector3& v) {
	return Math::Multiply(s, v);
}
Vector3 operator*(const Vector3& v, float s) {
	return s * v;
}
Vector3 operator/(const Vector3& v, float s) {
	return Math::Multiply(1.0f / s, v);
}
Matrix4x4 operator+(const Matrix4x4& m1, const Matrix4x4& m2) {
	return Math::Add(m1, m2);
}
Matrix4x4 operator-(const Matrix4x4& m1, const Matrix4x4& m2) {
	return Math::Subtract(m1, m2);
}
Matrix4x4 operator*(const Matrix4x4& m1, const Matrix4x4& m2) {
	return Math::Multiply(m1, m2);
}
// 単項演算子
Vector3 operator-(const Vector3& v) {
	return { -v.x,-v.y,-v.z };
}
Vector3 operator+(const Vector3& v) {
	return v;
}
// 複合代入演算子
Vector3& operator*=(Vector3& v, float s) {
	v.x *= s;
	v.y *= s;
	v.z *= s;
	return v;
}
Vector3& operator-=(Vector3& v1, const Vector3& v2) {
	v1.x -= v2.x;
	v1.y -= v2.y;
	v1.z -= v2.z;
	return v1;
}
Vector3& operator+=(Vector3& v1, const Vector3& v2) {
	v1.x += v2.x;
	v1.y += v2.y;
	v1.z += v2.z;
	return v1;
}
Vector3& operator/=(Vector3& v, float s) {
	v.x /= s;
	v.y /= s;
	v.z /= s;
	return v;
}
