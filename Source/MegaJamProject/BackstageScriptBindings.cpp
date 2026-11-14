#include "Core/AngelscriptBinds.h"
#include "Camera/CameraComponent.h"
#include "CanvasItem.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/LocalLightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Canvas.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/Actor.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/App.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "PaperSprite.h"
#include "PaperSpriteComponent.h"
#include "Styling/CoreStyle.h"
#include "TextureResource.h"
#include "UObject/Package.h"

namespace
{
	constexpr int32 BackstageTexSize = 128;
	constexpr float BackstageProjectionScale = 0.136f;
	constexpr float BackstageCameraDistance = 550.0f;
	constexpr int32 BackstageMaxCaptureSlots = 8;
	static int32 GBackstageSelectedLevel = 0;

	UTexture2D* CreateTransientTextureBGRA(int32 Width, int32 Height, const uint8* SrcBGRA, bool bSRGB = true)
	{
		if (Width <= 0 || Height <= 0 || !SrcBGRA)
		{
			return nullptr;
		}
		UTexture2D* Texture = UTexture2D::CreateTransient(Width, Height, PF_B8G8R8A8);
		if (!Texture)
		{
			return nullptr;
		}
		Texture->AddToRoot();
		Texture->SRGB = bSRGB;
		Texture->CompressionSettings = TC_EditorIcon;
		Texture->Filter = TF_Bilinear;
		Texture->AddressX = TA_Clamp;
		Texture->AddressY = TA_Clamp;
		Texture->MipGenSettings = TMGS_NoMipmaps;

		FTexturePlatformData* PlatformData = Texture->GetPlatformData();
		if (!PlatformData || PlatformData->Mips.Num() == 0)
		{
			return nullptr;
		}
		void* TextureData = PlatformData->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
		FMemory::Memcpy(TextureData, SrcBGRA, static_cast<SIZE_T>(Width) * Height * 4);
		PlatformData->Mips[0].BulkData.Unlock();
		Texture->UpdateResource();
		return Texture;
	}

	UTexture2D* LoadCustomArtTexture(const FString& FileName, bool bChromaKeyGreen)
	{
		static TMap<FName, TWeakObjectPtr<UTexture2D>> CachedCustomArt;
		const FName Key(*FString::Printf(TEXT("%s_%d"), *FileName, bChromaKeyGreen ? 1 : 0));
		if (TWeakObjectPtr<UTexture2D>* Existing = CachedCustomArt.Find(Key))
		{
			if (Existing->IsValid())
			{
				return Existing->Get();
			}
		}

		const FString FullPath = FPaths::ProjectContentDir() / TEXT("Backstage/Art") / FileName;
		TArray<uint8> FileBytes;
		if (!FFileHelper::LoadFileToArray(FileBytes, *FullPath) || FileBytes.Num() == 0)
		{
			return nullptr;
		}

		IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
		EImageFormat Format = ImageWrapperModule.DetectImageFormat(FileBytes.GetData(), FileBytes.Num());
		if (Format == EImageFormat::Invalid)
		{
			Format = FileName.EndsWith(TEXT(".png"), ESearchCase::IgnoreCase) ? EImageFormat::PNG : EImageFormat::JPEG;
		}
		TSharedPtr<IImageWrapper> Wrapper = ImageWrapperModule.CreateImageWrapper(Format);
		if (!Wrapper.IsValid() || !Wrapper->SetCompressed(FileBytes.GetData(), FileBytes.Num()))
		{
			return nullptr;
		}

		TArray<uint8> RawBGRA;
		if (!Wrapper->GetRaw(ERGBFormat::BGRA, 8, RawBGRA))
		{
			return nullptr;
		}

		int32 SrcW = Wrapper->GetWidth();
		int32 SrcH = Wrapper->GetHeight();
		if (SrcW <= 0 || SrcH <= 0 || RawBGRA.Num() < SrcW * SrcH * 4)
		{
			return nullptr;
		}

		UTexture2D* ResultTex = nullptr;
		if (bChromaKeyGreen)
		{
			int32 MinX = SrcW;
			int32 MaxX = -1;
			int32 MinY = SrcH;
			int32 MaxY = -1;

			for (int32 Y = 0; Y < SrcH; ++Y)
			{
				for (int32 X = 0; X < SrcW; ++X)
				{
					const int32 Idx = (Y * SrcW + X) * 4;
					uint8 B = RawBGRA[Idx + 0];
					uint8 G = RawBGRA[Idx + 1];
					uint8 R = RawBGRA[Idx + 2];
					const int32 MaxRB = FMath::Max<int32>(R, B);
					const int32 GreenExcess = static_cast<int32>(G) - MaxRB;
					if (G > 95 && GreenExcess > 24)
					{
						const float AlphaFrac = FMath::Clamp(1.0f - (static_cast<float>(GreenExcess) - 24.0f) / 36.0f, 0.0f, 1.0f);
						const uint8 Alpha = static_cast<uint8>(FMath::RoundToInt(AlphaFrac * 255.0f));
						RawBGRA[Idx + 3] = Alpha;
						if (G > MaxRB)
						{
							RawBGRA[Idx + 1] = static_cast<uint8>(MaxRB);
						}
					}
					if (RawBGRA[Idx + 3] > 24)
					{
						MinX = FMath::Min(MinX, X);
						MaxX = FMath::Max(MaxX, X);
						MinY = FMath::Min(MinY, Y);
						MaxY = FMath::Max(MaxY, Y);
					}
				}
			}

			if (MaxX > MinX && MaxY > MinY)
			{
				const int32 Pad = 6;
				MinX = FMath::Max(0, MinX - Pad);
				MaxX = FMath::Min(SrcW - 1, MaxX + Pad);
				MinY = FMath::Max(0, MinY - Pad);
				MaxY = FMath::Min(SrcH - 1, MaxY + 2);
				const int32 CropW = MaxX - MinX + 1;
				const int32 CropH = MaxY - MinY + 1;
				TArray<uint8> Cropped;
				Cropped.SetNumUninitialized(CropW * CropH * 4);
				for (int32 Y = 0; Y < CropH; ++Y)
				{
					FMemory::Memcpy(
						Cropped.GetData() + Y * CropW * 4,
						RawBGRA.GetData() + ((MinY + Y) * SrcW + MinX) * 4,
						CropW * 4);
				}
				ResultTex = CreateTransientTextureBGRA(CropW, CropH, Cropped.GetData(), true);
			}
		}

		if (!ResultTex)
		{
			ResultTex = CreateTransientTextureBGRA(SrcW, SrcH, RawBGRA.GetData(), true);
		}
		if (ResultTex)
		{
			CachedCustomArt.Add(Key, ResultTex);
		}
		return ResultTex;
	}

	UTexture2D* CreateCharityBackdropTexture()
	{
		static TWeakObjectPtr<UTexture2D> Cached;
		if (Cached.IsValid())
		{
			return Cached.Get();
		}

		const int32 W = 512;
		const int32 H = 288;
		TArray<uint8> Pixels;
		Pixels.SetNumUninitialized(W * H * 4);

		for (int32 Y = 0; Y < H; ++Y)
		{
			const float Ny = static_cast<float>(Y) / static_cast<float>(H - 1);
			for (int32 X = 0; X < W; ++X)
			{
				const float Nx = static_cast<float>(X) / static_cast<float>(W - 1);
				float R = FMath::Lerp(14.0f, 26.0f, Ny);
				float G = FMath::Lerp(22.0f, 38.0f, Ny);
				float B = FMath::Lerp(36.0f, 58.0f, Ny);

				// Velvet stage curtains on far left/right
				if (Nx < 0.14f || Nx > 0.86f)
				{
					const float Fold = 0.78f + 0.22f * FMath::Sin(Nx * 95.0f);
					R = 46.0f * Fold;
					G = 16.0f * Fold;
					B = 26.0f * Fold;
				}
				// Warm & cool stage spotlight cones
				const float BeamLeft = FMath::Exp(-FMath::Square((Nx - (0.34f + Ny * 0.08f)) * 9.0f)) * (1.0f - Ny * 0.45f);
				const float BeamRight = FMath::Exp(-FMath::Square((Nx - (0.66f - Ny * 0.08f)) * 9.0f)) * (1.0f - Ny * 0.45f);
				R += BeamLeft * 36.0f + BeamRight * 62.0f;
				G += BeamLeft * 74.0f + BeamRight * 56.0f;
				B += BeamLeft * 98.0f + BeamRight * 28.0f;

				// Subtle overhead rigging bar
				if (Ny > 0.08f && Ny < 0.12f)
				{
					R *= 0.55f; G *= 0.55f; B *= 0.55f;
				}

				const int32 Idx = (Y * W + X) * 4;
				Pixels[Idx + 0] = static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(B), 0, 255));
				Pixels[Idx + 1] = static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(G), 0, 255));
				Pixels[Idx + 2] = static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(R), 0, 255));
				Pixels[Idx + 3] = 255;
			}
		}

		UTexture2D* Tex = CreateTransientTextureBGRA(W, H, Pixels.GetData(), true);
		Cached = Tex;
		return Tex;
	}

	// Access protected UPaperSprite backing fields so runtime cutout sprites work without editor asset baking.
	struct FBackstagePaperSpriteAccessor : public UPaperSprite
	{
		void InitRuntimeSprite(UTexture2D* InTexture, UMaterialInterface* InMaterial, const FVector2D& SizeUU, const FVector2D& PivotNormalized)
		{
			SpriteCollisionDomain = ESpriteCollisionMode::None;
			AlternateMaterialSplitIndex = INDEX_NONE;
			DefaultMaterial = InMaterial;
			BakedSourceTexture = InTexture;
			BakedSourceUV = FVector2D::ZeroVector;
			BakedSourceDimension = InTexture
				? FVector2D(static_cast<double>(InTexture->GetSizeX()), static_cast<double>(InTexture->GetSizeY()))
				: FVector2D(64.0, 64.0);

#if WITH_EDITORONLY_DATA
			SourceTexture = InTexture;
			SourceUV = FVector2D::ZeroVector;
			SourceDimension = BakedSourceDimension;
			SourceTextureDimension = BakedSourceDimension;
#endif

			const float Left = static_cast<float>(-SizeUU.X * PivotNormalized.X);
			const float Right = static_cast<float>(SizeUU.X * (1.0 - PivotNormalized.X));
			const float Top = static_cast<float>(SizeUU.Y * PivotNormalized.Y);
			const float Bottom = static_cast<float>(-SizeUU.Y * (1.0 - PivotNormalized.Y));

			BakedRenderData.Reset(6);
			BakedRenderData.Emplace(Left, Top, 0.0f, 0.0f);
			BakedRenderData.Emplace(Left, Bottom, 0.0f, 1.0f);
			BakedRenderData.Emplace(Right, Bottom, 1.0f, 1.0f);
			BakedRenderData.Emplace(Left, Top, 0.0f, 0.0f);
			BakedRenderData.Emplace(Right, Bottom, 1.0f, 1.0f);
			BakedRenderData.Emplace(Right, Top, 1.0f, 0.0f);
		}
	};

	struct FBackstagePixelCanvas
	{
		int32 Width = BackstageTexSize;
		int32 Height = BackstageTexSize;
		int32 MinDirtyX = BackstageTexSize;
		int32 MaxDirtyX = -1;
		int32 MinDirtyY = BackstageTexSize;
		int32 MaxDirtyY = -1;
		TArray<FColor> Pixels;

		FBackstagePixelCanvas()
		{
			Pixels.Init(FColor(0, 0, 0, 0), Width * Height);
		}

		void SetPixel(int32 X, int32 Y, const FColor& Color)
		{
			if (X >= 0 && X < Width && Y >= 0 && Y < Height)
			{
				Pixels[Y * Width + X] = Color;
				if (Color.A > 0)
				{
					MinDirtyX = FMath::Min(MinDirtyX, X);
					MaxDirtyX = FMath::Max(MaxDirtyX, X);
					MinDirtyY = FMath::Min(MinDirtyY, Y);
					MaxDirtyY = FMath::Max(MaxDirtyY, Y);
				}
			}
		}

		FColor GetPixel(int32 X, int32 Y) const
		{
			if (X >= 0 && X < Width && Y >= 0 && Y < Height)
			{
				return Pixels[Y * Width + X];
			}
			return FColor(0, 0, 0, 0);
		}

		void FillEllipse(float Cx, float Cy, float Rx, float Ry, const FColor& Color)
		{
			if (Rx <= 0.0f || Ry <= 0.0f)
			{
				return;
			}
			const int32 MinX = FMath::Max(0, FMath::FloorToInt(Cx - Rx));
			const int32 MaxX = FMath::Min(Width - 1, FMath::CeilToInt(Cx + Rx));
			const int32 MinY = FMath::Max(0, FMath::FloorToInt(Cy - Ry));
			const int32 MaxY = FMath::Min(Height - 1, FMath::CeilToInt(Cy + Ry));
			for (int32 Y = MinY; Y <= MaxY; ++Y)
			{
				for (int32 X = MinX; X <= MaxX; ++X)
				{
					const float Nx = (X + 0.5f - Cx) / Rx;
					const float Ny = (Y + 0.5f - Cy) / Ry;
					if (Nx * Nx + Ny * Ny <= 1.0f)
					{
						SetPixel(X, Y, Color);
					}
				}
			}
		}

		void FillCircle(float Cx, float Cy, float Radius, const FColor& Color)
		{
			FillEllipse(Cx, Cy, Radius, Radius, Color);
		}

		void FillSoftEllipse(float Cx, float Cy, float Rx, float Ry, const FColor& Color)
		{
			const int32 MinX = FMath::Max(0, FMath::FloorToInt(Cx - Rx));
			const int32 MaxX = FMath::Min(Width - 1, FMath::CeilToInt(Cx + Rx));
			const int32 MinY = FMath::Max(0, FMath::FloorToInt(Cy - Ry));
			const int32 MaxY = FMath::Min(Height - 1, FMath::CeilToInt(Cy + Ry));
			for (int32 Y = MinY; Y <= MaxY; ++Y)
			{
				for (int32 X = MinX; X <= MaxX; ++X)
				{
					const float Nx = (X + 0.5f - Cx) / Rx;
					const float Ny = (Y + 0.5f - Cy) / Ry;
					const float DistSq = Nx * Nx + Ny * Ny;
					if (DistSq <= 1.0f)
					{
						const float Falloff = FMath::Pow(1.0f - DistSq, 1.4f);
						FColor Blended = Color;
						Blended.A = static_cast<uint8>(FMath::Clamp(Color.A * Falloff, 0.0f, 255.0f));
						SetPixel(X, Y, Blended);
					}
				}
			}
		}

		void FillRoundedBox(float X0, float Y0, float X1, float Y1, float Radius, const FColor& Color)
		{
			const int32 MinX = FMath::Max(0, FMath::FloorToInt(X0));
			const int32 MaxX = FMath::Min(Width - 1, FMath::CeilToInt(X1));
			const int32 MinY = FMath::Max(0, FMath::FloorToInt(Y0));
			const int32 MaxY = FMath::Min(Height - 1, FMath::CeilToInt(Y1));
			const float R = FMath::Min(Radius, FMath::Min((X1 - X0) * 0.5f, (Y1 - Y0) * 0.5f));
			for (int32 Y = MinY; Y <= MaxY; ++Y)
			{
				for (int32 X = MinX; X <= MaxX; ++X)
				{
					const float Px = X + 0.5f;
					const float Py = Y + 0.5f;
					const float ClampedX = FMath::Clamp(Px, X0 + R, X1 - R);
					const float ClampedY = FMath::Clamp(Py, Y0 + R, Y1 - R);
					const float Dx = Px - ClampedX;
					const float Dy = Py - ClampedY;
					if (Dx * Dx + Dy * Dy <= R * R)
					{
						SetPixel(X, Y, Color);
					}
				}
			}
		}

		void FillTaperedQuad(float TopLeft, float TopRight, float BottomLeft, float BottomRight, float TopY, float BottomY, const FColor& Color)
		{
			const int32 MinY = FMath::Max(0, FMath::FloorToInt(TopY));
			const int32 MaxY = FMath::Min(Height - 1, FMath::CeilToInt(BottomY));
			const float SpanY = FMath::Max(1.0f, BottomY - TopY);
			for (int32 Y = MinY; Y <= MaxY; ++Y)
			{
				const float Alpha = FMath::Clamp((Y + 0.5f - TopY) / SpanY, 0.0f, 1.0f);
				const float Left = FMath::Lerp(TopLeft, BottomLeft, Alpha);
				const float Right = FMath::Lerp(TopRight, BottomRight, Alpha);
				const int32 MinX = FMath::Max(0, FMath::FloorToInt(Left));
				const int32 MaxX = FMath::Min(Width - 1, FMath::CeilToInt(Right));
				for (int32 X = MinX; X <= MaxX; ++X)
				{
					SetPixel(X, Y, Color);
				}
			}
		}

		void FillSegment(float X0, float Y0, float X1, float Y1, float Radius, const FColor& Color)
		{
			const int32 MinX = FMath::Max(0, FMath::FloorToInt(FMath::Min(X0, X1) - Radius - 1.0f));
			const int32 MaxX = FMath::Min(Width - 1, FMath::CeilToInt(FMath::Max(X0, X1) + Radius + 1.0f));
			const int32 MinY = FMath::Max(0, FMath::FloorToInt(FMath::Min(Y0, Y1) - Radius - 1.0f));
			const int32 MaxY = FMath::Min(Height - 1, FMath::CeilToInt(FMath::Max(Y0, Y1) + Radius + 1.0f));
			const FVector2D A(X0, Y0);
			const FVector2D B(X1, Y1);
			const FVector2D AB = B - A;
			const float LenSq = AB.SizeSquared();
			const float RadiusSq = Radius * Radius;
			for (int32 Y = MinY; Y <= MaxY; ++Y)
			{
				for (int32 X = MinX; X <= MaxX; ++X)
				{
					const FVector2D P(X + 0.5f, Y + 0.5f);
					const float T = (LenSq > KINDA_SMALL_NUMBER) ? FMath::Clamp(FVector2D::DotProduct(P - A, AB) / LenSq, 0.0f, 1.0f) : 0.0f;
					const FVector2D Closest = A + AB * T;
					if ((P - Closest).SizeSquared() <= RadiusSq)
					{
						SetPixel(X, Y, Color);
					}
				}
			}
		}

		void FillTriangle(FVector2D A, FVector2D B, FVector2D C, const FColor& Color)
		{
			const int32 MinX = FMath::Max(0, FMath::FloorToInt(FMath::Min3(A.X, B.X, C.X)));
			const int32 MaxX = FMath::Min(Width - 1, FMath::CeilToInt(FMath::Max3(A.X, B.X, C.X)));
			const int32 MinY = FMath::Max(0, FMath::FloorToInt(FMath::Min3(A.Y, B.Y, C.Y)));
			const int32 MaxY = FMath::Min(Height - 1, FMath::CeilToInt(FMath::Max3(A.Y, B.Y, C.Y)));
			auto Edge = [](const FVector2D& P0, const FVector2D& P1, const FVector2D& P)
			{
				return (P.X - P0.X) * (P1.Y - P0.Y) - (P.Y - P0.Y) * (P1.X - P0.X);
			};
			const float Area = Edge(A, B, C);
			if (FMath::IsNearlyZero(Area))
			{
				return;
			}
			const float Sign = Area > 0.0f ? 1.0f : -1.0f;
			for (int32 Y = MinY; Y <= MaxY; ++Y)
			{
				for (int32 X = MinX; X <= MaxX; ++X)
				{
					const FVector2D P(X + 0.5f, Y + 0.5f);
					if (Edge(A, B, P) * Sign >= 0.0f && Edge(B, C, P) * Sign >= 0.0f && Edge(C, A, P) * Sign >= 0.0f)
					{
						SetPixel(X, Y, Color);
					}
				}
			}
		}

		// Adds a uniform dark ink contour around opaque silhouette pixels.
		void AddInkOutline(int32 Thickness, const FColor& InkColor)
		{
			if (MaxDirtyX < MinDirtyX || MaxDirtyY < MinDirtyY)
			{
				return;
			}
			TArray<FColor> Source = Pixels;
			const int32 RadiusSq = Thickness * Thickness;
			const int32 LoopMinY = FMath::Max(0, MinDirtyY - Thickness);
			const int32 LoopMaxY = FMath::Min(Height - 1, MaxDirtyY + Thickness);
			const int32 LoopMinX = FMath::Max(0, MinDirtyX - Thickness);
			const int32 LoopMaxX = FMath::Min(Width - 1, MaxDirtyX + Thickness);
			for (int32 Y = LoopMinY; Y <= LoopMaxY; ++Y)
			{
				for (int32 X = LoopMinX; X <= LoopMaxX; ++X)
				{
					if (Source[Y * Width + X].A >= 128)
					{
						continue;
					}

					bool bNearOpaque = false;
					for (int32 Dy = -Thickness; Dy <= Thickness && !bNearOpaque; ++Dy)
					{
						for (int32 Dx = -Thickness; Dx <= Thickness; ++Dx)
						{
							if (Dx * Dx + Dy * Dy > RadiusSq)
							{
								continue;
							}
							const int32 Nx = X + Dx;
							const int32 Ny = Y + Dy;
							if (Nx >= 0 && Nx < Width && Ny >= 0 && Ny < Height && Source[Ny * Width + Nx].A >= 128)
							{
								bNearOpaque = true;
								break;
							}
						}
					}
					if (bNearOpaque)
					{
						SetPixel(X, Y, InkColor);
					}
				}
			}
		}
	};

	void DrawHeadTexture(FBackstagePixelCanvas& Canvas, const FName& PartKind)
	{
		const FColor Ink(16, 20, 28, 255);
		const FColor Skin(244, 224, 210, 255);
		const FColor SkinShadow(214, 186, 168, 255);
		const FColor Hair(38, 44, 58, 255);
		const FColor HairHighlight(78, 90, 114, 255);

		// Base skull and chin silhouette
		Canvas.FillEllipse(64.0f, 66.0f, 30.0f, 34.0f, Skin);
		Canvas.FillTaperedQuad(35.0f, 93.0f, 44.0f, 84.0f, 68.0f, 98.0f, Skin);
		Canvas.FillEllipse(50.0f, 75.0f, 12.0f, 20.0f, SkinShadow);

		// Stylized Limbus-style layered hair crown and side bangs
		Canvas.FillEllipse(62.0f, 46.0f, 35.0f, 25.0f, Hair);
		Canvas.FillTriangle(FVector2D(28.0f, 52.0f), FVector2D(44.0f, 34.0f), FVector2D(36.0f, 84.0f), Hair);
		Canvas.FillTriangle(FVector2D(98.0f, 52.0f), FVector2D(82.0f, 34.0f), FVector2D(90.0f, 78.0f), Hair);
		Canvas.FillTriangle(FVector2D(44.0f, 36.0f), FVector2D(64.0f, 38.0f), FVector2D(54.0f, 62.0f), Hair);
		Canvas.FillTriangle(FVector2D(62.0f, 34.0f), FVector2D(84.0f, 38.0f), FVector2D(76.0f, 60.0f), Hair);
		Canvas.FillTriangle(FVector2D(54.0f, 22.0f), FVector2D(66.0f, 32.0f), FVector2D(42.0f, 38.0f), Hair);
		Canvas.AddInkOutline(4, Ink);

		// Subtle hair sheen
		Canvas.FillSegment(46.0f, 38.0f, 60.0f, 34.0f, 2.0f, HairHighlight);
		Canvas.FillSegment(66.0f, 34.0f, 80.0f, 39.0f, 2.0f, HairHighlight);

		// Eyes & expression details (facing slightly right)
		const float EyeY = 67.0f;
		const float LeftEyeX = 56.0f;
		const float RightEyeX = 78.0f;

		if (PartKind == FName(TEXT("Head_Shock")))
		{
			Canvas.FillCircle(LeftEyeX, EyeY, 5.5f, FColor(250, 252, 255, 255));
			Canvas.FillCircle(RightEyeX, EyeY, 5.5f, FColor(250, 252, 255, 255));
			Canvas.FillCircle(LeftEyeX + 1.0f, EyeY, 2.2f, Ink);
			Canvas.FillCircle(RightEyeX + 1.0f, EyeY, 2.2f, Ink);
			Canvas.FillSegment(49.0f, 56.0f, 60.0f, 53.0f, 1.8f, Ink);
			Canvas.FillSegment(72.0f, 53.0f, 84.0f, 56.0f, 1.8f, Ink);
			Canvas.FillEllipse(68.0f, 86.0f, 4.5f, 5.5f, Ink);
		}
		else if (PartKind == FName(TEXT("Head_Angry")))
		{
			Canvas.FillEllipse(LeftEyeX, EyeY + 1.0f, 5.0f, 3.6f, Ink);
			Canvas.FillEllipse(RightEyeX, EyeY + 1.0f, 5.0f, 3.6f, Ink);
			Canvas.FillCircle(LeftEyeX + 1.5f, EyeY, 1.4f, FColor(250, 252, 255, 255));
			Canvas.FillCircle(RightEyeX + 1.5f, EyeY, 1.4f, FColor(250, 252, 255, 255));
			Canvas.FillSegment(48.0f, 56.0f, 62.0f, 62.0f, 2.2f, Ink);
			Canvas.FillSegment(85.0f, 56.0f, 71.0f, 62.0f, 2.2f, Ink);
			Canvas.FillSegment(61.0f, 85.0f, 75.0f, 83.0f, 2.0f, Ink);
		}
		else if (PartKind == FName(TEXT("Head_Focus")))
		{
			Canvas.FillSegment(50.0f, EyeY + 1.0f, 61.0f, EyeY + 3.0f, 2.2f, Ink);
			Canvas.FillSegment(72.0f, EyeY + 3.0f, 83.0f, EyeY + 1.0f, 2.2f, Ink);
			Canvas.FillSegment(50.0f, 58.0f, 61.0f, 57.0f, 1.6f, Ink);
			Canvas.FillSegment(72.0f, 57.0f, 83.0f, 58.0f, 1.6f, Ink);
			Canvas.FillSegment(63.0f, 85.0f, 72.0f, 85.0f, 1.6f, Ink);
		}
		else if (PartKind == FName(TEXT("Head_Warm")))
		{
			Canvas.FillSegment(50.0f, EyeY + 1.0f, 56.0f, EyeY - 3.0f, 2.0f, Ink);
			Canvas.FillSegment(56.0f, EyeY - 3.0f, 62.0f, EyeY + 1.0f, 2.0f, Ink);
			Canvas.FillSegment(72.0f, EyeY + 1.0f, 78.0f, EyeY - 3.0f, 2.0f, Ink);
			Canvas.FillSegment(78.0f, EyeY - 3.0f, 84.0f, EyeY + 1.0f, 2.0f, Ink);
			Canvas.FillSegment(61.0f, 83.0f, 67.0f, 87.0f, 1.8f, Ink);
			Canvas.FillSegment(67.0f, 87.0f, 74.0f, 83.0f, 1.8f, Ink);
		}
		else
		{
			Canvas.FillEllipse(LeftEyeX, EyeY, 4.4f, 4.8f, Ink);
			Canvas.FillEllipse(RightEyeX, EyeY, 4.4f, 4.8f, Ink);
			Canvas.FillCircle(LeftEyeX + 1.2f, EyeY - 1.2f, 1.5f, FColor(250, 252, 255, 255));
			Canvas.FillCircle(RightEyeX + 1.2f, EyeY - 1.2f, 1.5f, FColor(250, 252, 255, 255));
			Canvas.FillSegment(50.0f, 57.0f, 61.0f, 56.0f, 1.7f, Ink);
			Canvas.FillSegment(72.0f, 56.0f, 83.0f, 57.0f, 1.7f, Ink);
			Canvas.FillSegment(63.0f, 85.0f, 72.0f, 85.0f, 1.6f, Ink);
		}
	}

	void DrawTorsoTexture(FBackstagePixelCanvas& Canvas, const FName& PartKind)
	{
		const FColor Ink(16, 20, 28, 255);
		const FColor Base(246, 246, 250, 255);
		const FColor Shade(182, 188, 202, 255);
		const FColor Shirt(236, 240, 248, 255);

		if (PartKind == FName(TEXT("Torso_Coat")))
		{
			Canvas.FillTaperedQuad(36.0f, 92.0f, 22.0f, 106.0f, 18.0f, 114.0f, Base);
			Canvas.FillTaperedQuad(36.0f, 54.0f, 22.0f, 44.0f, 18.0f, 114.0f, Shade);
		}
		else if (PartKind == FName(TEXT("Torso_Vest")))
		{
			Canvas.FillTaperedQuad(38.0f, 90.0f, 34.0f, 94.0f, 18.0f, 102.0f, Base);
			Canvas.FillTaperedQuad(38.0f, 54.0f, 34.0f, 50.0f, 18.0f, 102.0f, Shade);
		}
		else
		{
			Canvas.FillTaperedQuad(36.0f, 92.0f, 32.0f, 96.0f, 18.0f, 108.0f, Base);
			Canvas.FillTaperedQuad(36.0f, 52.0f, 32.0f, 48.0f, 18.0f, 108.0f, Shade);
		}
		Canvas.AddInkOutline(4, Ink);

		// Lapel collar & inner shirt
		Canvas.FillTriangle(FVector2D(52.0f, 20.0f), FVector2D(76.0f, 20.0f), FVector2D(66.0f, 50.0f), Shirt);
		Canvas.FillSegment(52.0f, 20.0f, 66.0f, 52.0f, 2.2f, Ink);
		Canvas.FillSegment(76.0f, 20.0f, 66.0f, 52.0f, 2.2f, Ink);

		if (PartKind == FName(TEXT("Torso_Vest")))
		{
			Canvas.FillRoundedBox(36.0f, 66.0f, 92.0f, 76.0f, 2.0f, Shirt);
			Canvas.FillSegment(36.0f, 66.0f, 92.0f, 66.0f, 1.5f, Ink);
			Canvas.FillSegment(36.0f, 76.0f, 92.0f, 76.0f, 1.5f, Ink);
		}
		else
		{
			// Belt / seam and lapel pin
			Canvas.FillSegment(32.0f, 74.0f, 96.0f, 74.0f, 2.2f, Ink);
			Canvas.FillCircle(78.0f, 44.0f, 4.0f, Shirt);
		}
	}

	void DrawEmotionTexture(FBackstagePixelCanvas& Canvas, const FName& PartKind)
	{
		const FColor Ink(16, 20, 28, 255);
		if (PartKind == FName(TEXT("Emotion_Shock")))
		{
			const FColor Bright(255, 232, 120, 255);
			Canvas.FillSegment(64.0f, 20.0f, 64.0f, 76.0f, 9.0f, Bright);
			Canvas.FillCircle(64.0f, 100.0f, 9.5f, Bright);
			Canvas.FillTriangle(FVector2D(34.0f, 34.0f), FVector2D(46.0f, 44.0f), FVector2D(28.0f, 50.0f), Bright);
			Canvas.FillTriangle(FVector2D(94.0f, 34.0f), FVector2D(82.0f, 44.0f), FVector2D(100.0f, 50.0f), Bright);
			Canvas.AddInkOutline(4, Ink);
		}
		else if (PartKind == FName(TEXT("Emotion_Anger")))
		{
			const FColor Red(248, 88, 108, 255);
			Canvas.FillSegment(32.0f, 42.0f, 54.0f, 54.0f, 5.5f, Red);
			Canvas.FillSegment(54.0f, 54.0f, 42.0f, 32.0f, 5.5f, Red);
			Canvas.FillSegment(96.0f, 42.0f, 74.0f, 54.0f, 5.5f, Red);
			Canvas.FillSegment(74.0f, 54.0f, 86.0f, 32.0f, 5.5f, Red);
			Canvas.FillSegment(32.0f, 86.0f, 54.0f, 74.0f, 5.5f, Red);
			Canvas.FillSegment(54.0f, 74.0f, 42.0f, 96.0f, 5.5f, Red);
			Canvas.FillSegment(96.0f, 86.0f, 74.0f, 74.0f, 5.5f, Red);
			Canvas.FillSegment(74.0f, 74.0f, 86.0f, 96.0f, 5.5f, Red);
			Canvas.AddInkOutline(4, Ink);
		}
		else if (PartKind == FName(TEXT("Emotion_Care")))
		{
			const FColor Blue(110, 210, 255, 255);
			Canvas.FillCircle(64.0f, 78.0f, 22.0f, Blue);
			Canvas.FillTriangle(FVector2D(64.0f, 22.0f), FVector2D(44.0f, 70.0f), FVector2D(84.0f, 70.0f), Blue);
			Canvas.AddInkOutline(4, Ink);
			Canvas.FillEllipse(55.0f, 74.0f, 4.0f, 9.0f, FColor(236, 250, 255, 255));
		}
		else
		{
			const FColor Mint(110, 242, 196, 255);
			Canvas.FillTriangle(FVector2D(64.0f, 18.0f), FVector2D(52.0f, 64.0f), FVector2D(76.0f, 64.0f), Mint);
			Canvas.FillTriangle(FVector2D(64.0f, 110.0f), FVector2D(52.0f, 64.0f), FVector2D(76.0f, 64.0f), Mint);
			Canvas.FillTriangle(FVector2D(18.0f, 64.0f), FVector2D(64.0f, 52.0f), FVector2D(64.0f, 76.0f), Mint);
			Canvas.FillTriangle(FVector2D(110.0f, 64.0f), FVector2D(64.0f, 52.0f), FVector2D(64.0f, 76.0f), Mint);
			Canvas.AddInkOutline(4, Ink);
		}
	}

	UTexture2D* CreateTextureFromCanvas(const FBackstagePixelCanvas& Canvas)
	{
		UTexture2D* Texture = UTexture2D::CreateTransient(BackstageTexSize, BackstageTexSize, PF_B8G8R8A8);
		if (!Texture)
		{
			return nullptr;
		}
		Texture->AddToRoot();
		Texture->SRGB = true;
		Texture->Filter = TF_Bilinear;
		Texture->AddressX = TA_Clamp;
		Texture->AddressY = TA_Clamp;
		Texture->NeverStream = true;

		if (FTexturePlatformData* PlatformData = Texture->GetPlatformData(); PlatformData && PlatformData->Mips.Num() > 0)
		{
			if (void* Data = PlatformData->Mips[0].BulkData.Lock(LOCK_READ_WRITE))
			{
				FMemory::Memcpy(Data, Canvas.Pixels.GetData(), Canvas.Pixels.Num() * sizeof(FColor));
				PlatformData->Mips[0].BulkData.Unlock();
			}
		}

		if (!IsRunningCommandlet() && FApp::CanEverRender())
		{
			Texture->UpdateResource();
		}
		return Texture;
	}

	UTexture2D* GetOrCreatePartTexture(const FName& PartKind)
	{
		static TMap<FName, TWeakObjectPtr<UTexture2D>> CachedTextures;
		if (TWeakObjectPtr<UTexture2D>* Existing = CachedTextures.Find(PartKind))
		{
			if (Existing->IsValid())
			{
				return Existing->Get();
			}
		}

		FBackstagePixelCanvas Canvas;
		const FColor Ink(16, 20, 28, 255);
		const FColor Base(246, 246, 250, 255);
		const FColor Shade(182, 188, 202, 255);

		if (PartKind == FName(TEXT("Shadow")))
		{
			Canvas.FillSoftEllipse(64.0f, 64.0f, 54.0f, 28.0f, FColor(6, 10, 16, 180));
		}
		else if (PartKind == FName(TEXT("Parcel")))
		{
			Canvas.FillRoundedBox(20.0f, 24.0f, 108.0f, 104.0f, 8.0f, Base);
			Canvas.FillRoundedBox(20.0f, 72.0f, 108.0f, 104.0f, 6.0f, Shade);
			Canvas.AddInkOutline(4, Ink);
			Canvas.FillRoundedBox(58.0f, 26.0f, 70.0f, 102.0f, 2.0f, FColor(255, 212, 128, 255));
			Canvas.FillRoundedBox(22.0f, 52.0f, 106.0f, 64.0f, 2.0f, FColor(255, 212, 128, 255));
			Canvas.FillSegment(22.0f, 52.0f, 106.0f, 52.0f, 1.5f, Ink);
		}
		else if (PartKind == FName(TEXT("Limb")))
		{
			Canvas.FillTaperedQuad(46.0f, 82.0f, 52.0f, 76.0f, 16.0f, 106.0f, Base);
			Canvas.FillTaperedQuad(46.0f, 60.0f, 52.0f, 60.0f, 16.0f, 106.0f, Shade);
			Canvas.FillCircle(64.0f, 106.0f, 12.0f, Base);
			Canvas.AddInkOutline(4, Ink);
			Canvas.FillSegment(50.0f, 88.0f, 78.0f, 88.0f, 2.0f, Ink);
		}
		else if (PartKind == FName(TEXT("Torso_Coat")) || PartKind == FName(TEXT("Torso_Vest")) || PartKind == FName(TEXT("Torso_Suit")))
		{
			DrawTorsoTexture(Canvas, PartKind);
		}
		else if (PartKind == FName(TEXT("Umbrella")))
		{
			const FColor Canopy(52, 82, 118, 255);
			const FColor CanopyLight(92, 134, 182, 255);
			const FColor RimGold(246, 206, 118, 255);
			const FColor HandleColor(176, 128, 86, 255);
			Canvas.FillSegment(64.0f, 34.0f, 64.0f, 108.0f, 2.5f, HandleColor);
			Canvas.FillSegment(64.0f, 108.0f, 72.0f, 114.0f, 2.5f, HandleColor);
			Canvas.FillEllipse(64.0f, 52.0f, 50.0f, 22.0f, Canopy);
			Canvas.FillEllipse(64.0f, 46.0f, 36.0f, 14.0f, CanopyLight);
			for (int32 Y = 54; Y < BackstageTexSize; ++Y)
			{
				for (int32 X = 0; X < BackstageTexSize; ++X)
				{
					if (X < 61 || X > 67 || Y > 116)
					{
						Canvas.Pixels[Y * Canvas.Width + X] = FColor(0, 0, 0, 0);
					}
				}
			}
			Canvas.FillSegment(16.0f, 53.0f, 112.0f, 53.0f, 2.0f, RimGold);
			Canvas.FillCircle(64.0f, 28.0f, 3.0f, RimGold);
			Canvas.AddInkOutline(3, Ink);
		}
		else if (PartKind == FName(TEXT("UmbrellaFolded")))
		{
			const FColor Canopy(52, 82, 118, 255);
			const FColor HandleColor(176, 128, 86, 255);
			Canvas.FillSegment(64.0f, 22.0f, 64.0f, 106.0f, 2.5f, HandleColor);
			Canvas.FillTaperedQuad(59.0f, 69.0f, 62.0f, 66.0f, 32.0f, 92.0f, Canopy);
			Canvas.AddInkOutline(3, Ink);
		}
		else if (PartKind == FName(TEXT("FanSign")))
		{
			const FColor Board(252, 232, 240, 255);
			const FColor RimPink(248, 96, 146, 255);
			const FColor HeartPink(238, 62, 108, 255);
			Canvas.FillSegment(54.0f, 76.0f, 54.0f, 114.0f, 2.8f, Shade);
			Canvas.FillSegment(74.0f, 76.0f, 74.0f, 114.0f, 2.8f, Shade);
			Canvas.FillRoundedBox(20.0f, 24.0f, 108.0f, 82.0f, 8.0f, RimPink);
			Canvas.FillRoundedBox(26.0f, 30.0f, 102.0f, 76.0f, 5.0f, Board);
			Canvas.FillCircle(57.0f, 48.0f, 7.5f, HeartPink);
			Canvas.FillCircle(71.0f, 48.0f, 7.5f, HeartPink);
			Canvas.FillTriangle(FVector2D(49.0f, 50.0f), FVector2D(79.0f, 50.0f), FVector2D(64.0f, 68.0f), HeartPink);
			Canvas.FillSegment(34.0f, 64.0f, 50.0f, 64.0f, 2.0f, RimPink);
			Canvas.FillSegment(78.0f, 64.0f, 94.0f, 64.0f, 2.0f, RimPink);
			Canvas.AddInkOutline(3, Ink);
		}
		else if (PartKind == FName(TEXT("RainStreak")))
		{
			const FColor DropColor(186, 224, 255, 195);
			const FColor DropCore(232, 246, 255, 225);
			Canvas.FillSegment(74.0f, 18.0f, 54.0f, 110.0f, 2.0f, DropColor);
			Canvas.FillSegment(68.0f, 42.0f, 58.0f, 92.0f, 1.1f, DropCore);
		}
		else if (PartKind == FName(TEXT("BrandBanner")))
		{
			const FColor FrameDark(28, 38, 56, 255);
			const FColor GlowCyan(88, 226, 210, 255);
			const FColor GlowGold(252, 206, 112, 255);
			Canvas.FillRoundedBox(14.0f, 22.0f, 114.0f, 106.0f, 6.0f, FrameDark);
			Canvas.FillRoundedBox(20.0f, 28.0f, 108.0f, 100.0f, 4.0f, FColor(40, 56, 82, 255));
			Canvas.FillTriangle(FVector2D(64.0f, 36.0f), FVector2D(46.0f, 76.0f), FVector2D(82.0f, 76.0f), GlowCyan);
			Canvas.FillCircle(64.0f, 64.0f, 12.0f, GlowGold);
			Canvas.FillSegment(28.0f, 88.0f, 100.0f, 88.0f, 2.2f, GlowCyan);
			Canvas.AddInkOutline(3, Ink);
		}
		else if (PartKind == FName(TEXT("Backdrop_Rain")))
		{
			UTexture2D* CustomTex = LoadCustomArtTexture(TEXT("bg_rain_event.jpg"), false);
			if (!CustomTex)
			{
				CustomTex = LoadCustomArtTexture(TEXT("bg_rain_event.png"), false);
			}
			if (!CustomTex)
			{
				CustomTex = CreateCharityBackdropTexture();
			}
			if (CustomTex)
			{
				CachedTextures.Add(PartKind, CustomTex);
			}
			return CustomTex;
		}
		else if (PartKind == FName(TEXT("Backdrop_Charity")))
		{
			UTexture2D* CustomTex = LoadCustomArtTexture(TEXT("bg_charity_backstage.jpg"), false);
			if (!CustomTex)
			{
				CustomTex = CreateCharityBackdropTexture();
			}
			if (CustomTex)
			{
				CachedTextures.Add(PartKind, CustomTex);
			}
			return CustomTex;
		}
		else if (PartKind == FName(TEXT("Emotion_Shock")) || PartKind == FName(TEXT("Emotion_Anger")) || PartKind == FName(TEXT("Emotion_Care")) || PartKind == FName(TEXT("Emotion_Warm")))
		{
			DrawEmotionTexture(Canvas, PartKind);
		}
		else
		{
			DrawHeadTexture(Canvas, PartKind);
		}

		UTexture2D* Texture = CreateTextureFromCanvas(Canvas);
		if (Texture)
		{
			CachedTextures.Add(PartKind, Texture);
		}
		return Texture;
	}

	void DrawCharacterSequenceFrame(FBackstagePixelCanvas& Canvas, const FName& SequenceName, int32 FrameIndex)
	{
		const FString SeqStr = SequenceName.ToString();
		const int32 Frame = FMath::Clamp(FrameIndex, 0, 3);

		const bool bVolunteer = SeqStr.StartsWith(TEXT("Volunteer_")) || SeqStr.StartsWith(TEXT("Fan_"));
		const bool bCoordinator = SeqStr.StartsWith(TEXT("Coordinator_")) || SeqStr.StartsWith(TEXT("XiaoBei_"));
		const bool bLinWan = SeqStr.StartsWith(TEXT("LinWan_"));
		const bool bXiaoBei = SeqStr.StartsWith(TEXT("XiaoBei_"));
		const bool bFan = SeqStr.StartsWith(TEXT("Fan_"));

		const FColor Ink(16, 20, 28, 255);
		const FColor Skin(244, 224, 210, 255);
		const FColor SkinShade(214, 184, 166, 255);
		const FColor Shirt(236, 241, 248, 255);
		const FColor PantsFront = bVolunteer ? FColor(48, 66, 94, 255) : FColor(34, 39, 52, 255);
		const FColor PantsBack(24, 28, 38, 255);
		const FColor Boot(20, 24, 32, 255);

		FColor Primary(235, 86, 112, 255);
		FColor PrimaryShade(166, 48, 72, 255);
		FColor Accent(252, 214, 128, 255);
		FColor Hair(36, 38, 52, 255);
		FColor HairSheen(82, 88, 116, 255);

		if (bLinWan)
		{
			Primary = FColor(224, 62, 108, 255);
			PrimaryShade = FColor(152, 34, 70, 255);
			Accent = FColor(250, 224, 178, 255);
			Hair = FColor(40, 26, 34, 255);
			HairSheen = FColor(112, 74, 92, 255);
		}
		else if (bXiaoBei)
		{
			Primary = FColor(58, 152, 168, 255);
			PrimaryShade = FColor(36, 102, 118, 255);
			Accent = FColor(238, 202, 122, 255);
			Hair = FColor(34, 42, 54, 255);
			HairSheen = FColor(74, 92, 116, 255);
		}
		else if (bFan)
		{
			Primary = FColor(118, 146, 242, 255);
			PrimaryShade = FColor(78, 102, 192, 255);
			Accent = FColor(250, 146, 188, 255);
			Hair = FColor(62, 48, 54, 255);
			HairSheen = FColor(118, 92, 106, 255);
		}
		else if (bVolunteer)
		{
			Primary = FColor(72, 162, 246, 255);
			PrimaryShade = FColor(44, 112, 188, 255);
			Accent = FColor(246, 248, 252, 255);
			Hair = FColor(56, 46, 48, 255);
			HairSheen = FColor(102, 84, 88, 255);
		}
		else if (bCoordinator)
		{
			Primary = FColor(84, 208, 152, 255);
			PrimaryShade = FColor(48, 146, 102, 255);
			Accent = FColor(28, 38, 48, 255);
			Hair = FColor(32, 44, 52, 255);
			HairSheen = FColor(72, 98, 112, 255);
		}

		const bool bWalk = SeqStr.EndsWith(TEXT("_Walk"));
		const bool bShock = SeqStr.EndsWith(TEXT("_Shock"));
		const bool bArgue = SeqStr.EndsWith(TEXT("_Argue"));
		const bool bPickup = SeqStr.EndsWith(TEXT("_Pickup"));
		const bool bCare = SeqStr.EndsWith(TEXT("_Care"));
		const bool bHandoff = SeqStr.EndsWith(TEXT("_Handoff"));
		const bool bWarm = SeqStr.EndsWith(TEXT("_Warm"));

		float DropY = 0.0f;
		float LeanX = 0.0f;
		float HeadNodY = 0.0f;
		float BackFootX = -7.0f;
		float FrontFootX = 8.0f;
		float KneeFoldY = 0.0f;
		FVector2D BackHandOffset(12.0f, 18.0f);
		FVector2D FrontHandOffset(-6.0f, 22.0f);

		if (bWalk)
		{
			const float LegSwing[] = { 13.0f, 3.0f, -13.0f, -3.0f };
			const float BobY[] = { -1.5f, 1.5f, -1.5f, 1.5f };
			BackFootX = -LegSwing[Frame];
			FrontFootX = LegSwing[Frame];
			LeanX = 3.5f;
			HeadNodY = BobY[Frame];
			DropY = BobY[Frame];
			BackHandOffset = FVector2D(-LegSwing[Frame] * 0.9f + 4.0f, 19.0f);
			FrontHandOffset = FVector2D(LegSwing[Frame] * 0.9f, 20.0f);
		}
		else if (bShock)
		{
			const float JoltX[] = { -7.0f, -10.0f, -6.0f, -8.0f };
			const float JumpY[] = { -1.0f, -4.0f, -1.5f, 0.0f };
			LeanX = JoltX[Frame];
			DropY = JumpY[Frame];
			BackFootX = -11.0f;
			FrontFootX = 11.0f;
			BackHandOffset = FVector2D(18.0f, -12.0f - (Frame == 1 ? 3.0f : 0.0f));
			FrontHandOffset = FVector2D(14.0f, -10.0f + (Frame == 2 ? 2.0f : 0.0f));
		}
		else if (bArgue)
		{
			const float ThrustX[] = { 7.0f, 10.0f, 8.0f, 5.0f };
			const float ChopY[] = { -8.0f, 0.0f, 12.0f, -4.0f };
			LeanX = ThrustX[Frame];
			HeadNodY = (Frame == 1) ? -1.0f : 1.0f;
			BackFootX = -10.0f;
			FrontFootX = 12.0f;
			BackHandOffset = FVector2D(10.0f, 16.0f);
			FrontHandOffset = FVector2D(26.0f + (Frame == 1 ? 5.0f : 0.0f), ChopY[Frame]);
		}
		else if (bPickup)
		{
			const float ReachDx[] = { 24.0f, 29.0f, 27.0f, 25.0f };
			const float ReachDy[] = { 24.0f, 28.0f, 26.0f, 23.0f };
			DropY = 26.0f + (Frame % 2 == 1 ? 1.5f : 0.0f);
			LeanX = 11.0f;
			HeadNodY = 3.0f;
			KneeFoldY = 12.0f;
			BackFootX = -15.0f;
			FrontFootX = 10.0f;
			BackHandOffset = FVector2D(6.0f, 16.0f);
			FrontHandOffset = FVector2D(ReachDx[Frame], ReachDy[Frame]);
		}
		else if (bCare)
		{
			const float Breathe[] = { 0.0f, 1.0f, 2.0f, 1.0f };
			LeanX = 5.0f;
			HeadNodY = Breathe[Frame];
			BackFootX = -7.0f;
			FrontFootX = 9.0f;
			BackHandOffset = FVector2D(16.0f, 11.0f);
			FrontHandOffset = FVector2D(22.0f, 8.0f + Breathe[Frame]);
		}
		else if (bHandoff)
		{
			const float ReachOut[] = { 21.0f, 25.0f, 24.0f, 22.0f };
			LeanX = 4.5f;
			HeadNodY = (Frame == 1) ? 1.5f : 0.0f;
			BackFootX = -8.0f;
			FrontFootX = 9.0f;
			BackHandOffset = FVector2D(ReachOut[Frame] + 2.0f, 7.0f);
			FrontHandOffset = FVector2D(ReachOut[Frame], 10.0f);
		}
		else if (bWarm)
		{
			const float WaveY[] = { -11.0f, -15.0f, -10.0f, -13.0f };
			const float WaveX[] = { 17.0f, 20.0f, 15.0f, 18.0f };
			DropY = (Frame == 1 || Frame == 3) ? -1.5f : 0.0f;
			LeanX = 1.5f;
			BackFootX = -6.0f;
			FrontFootX = 8.0f;
			BackHandOffset = FVector2D(-10.0f, 18.0f);
			FrontHandOffset = FVector2D(WaveX[Frame], WaveY[Frame]);
		}
		else
		{
			// Idle breathing & subtle stance sway
			const float BreathY[] = { 0.0f, 1.0f, 1.8f, 1.0f };
			DropY = BreathY[Frame] * 0.5f;
			HeadNodY = BreathY[Frame];
			BackFootX = -6.5f;
			FrontFootX = 7.5f;
			BackHandOffset = FVector2D(10.0f, 20.0f);
			FrontHandOffset = FVector2D(-9.0f, 22.0f);
		}

		const float PelvisX = 63.0f + LeanX * 0.35f;
		const float PelvisY = 82.0f + DropY;
		const float ShoulderX = 63.0f + LeanX;
		const float ShoulderY = 53.0f + DropY;
		const float HeadX = 64.0f + LeanX * 1.1f;
		const float HeadY = 33.0f + DropY + HeadNodY;
		const float FootBaseY = 120.0f;

		// Back arm (darker depth shade)
		Canvas.FillSegment(ShoulderX + 6.0f, ShoulderY + 3.0f, ShoulderX + BackHandOffset.X, ShoulderY + BackHandOffset.Y, 4.2f, PrimaryShade);
		Canvas.FillCircle(ShoulderX + BackHandOffset.X, ShoulderY + BackHandOffset.Y, 4.3f, SkinShade);

		// Back leg
		const float BackKneeX = FMath::Lerp(PelvisX - 2.0f, 63.0f + BackFootX, 0.55f) - (bPickup ? 6.0f : 0.0f);
		const float BackKneeY = FMath::Lerp(PelvisY, FootBaseY, 0.5f) + KneeFoldY * 0.5f;
		Canvas.FillSegment(PelvisX - 2.0f, PelvisY, BackKneeX, BackKneeY, 5.2f, PantsBack);
		Canvas.FillSegment(BackKneeX, BackKneeY, 63.0f + BackFootX, FootBaseY, 4.8f, PantsBack);
		Canvas.FillRoundedBox(63.0f + BackFootX - 4.0f, FootBaseY - 3.0f, 63.0f + BackFootX + 8.0f, FootBaseY + 3.0f, 2.0f, Boot);

		// Front leg
		const float FrontKneeX = FMath::Lerp(PelvisX + 2.0f, 63.0f + FrontFootX, 0.55f) + (bPickup ? 6.0f : 0.0f);
		const float FrontKneeY = FMath::Lerp(PelvisY, FootBaseY, 0.5f) - (bPickup ? 5.0f : 0.0f);
		Canvas.FillSegment(PelvisX + 2.0f, PelvisY, FrontKneeX, FrontKneeY, 5.5f, PantsFront);
		Canvas.FillSegment(FrontKneeX, FrontKneeY, 63.0f + FrontFootX, FootBaseY, 5.0f, PantsFront);
		Canvas.FillRoundedBox(63.0f + FrontFootX - 4.0f, FootBaseY - 3.0f, 63.0f + FrontFootX + 8.5f, FootBaseY + 3.0f, 2.0f, Boot);

		// Torso & signature outerwear silhouette
		if (!bVolunteer && !bCoordinator)
		{
			// Artist long coat flare
			const float Flare = bArgue ? -5.0f : (bWalk ? -3.0f : 0.0f);
			Canvas.FillTaperedQuad(ShoulderX - 11.0f, ShoulderX + 11.0f, PelvisX - 16.0f + Flare, PelvisX + 15.0f, ShoulderY - 2.0f, PelvisY + 13.0f, Primary);
			Canvas.FillTaperedQuad(ShoulderX - 11.0f, ShoulderX - 2.0f, PelvisX - 16.0f + Flare, PelvisX - 4.0f, ShoulderY - 2.0f, PelvisY + 13.0f, PrimaryShade);
		}
		else if (bVolunteer)
		{
			// Volunteer utility vest over crisp shirt
			Canvas.FillTaperedQuad(ShoulderX - 10.5f, ShoulderX + 10.5f, PelvisX - 10.0f, PelvisX + 10.0f, ShoulderY - 2.0f, PelvisY + 3.0f, Primary);
			Canvas.FillTaperedQuad(ShoulderX - 10.5f, ShoulderX - 3.0f, PelvisX - 10.0f, PelvisX - 3.0f, ShoulderY - 2.0f, PelvisY + 3.0f, PrimaryShade);
		}
		else
		{
			// Coordinator tailored blazer
			Canvas.FillTaperedQuad(ShoulderX - 11.0f, ShoulderX + 11.0f, PelvisX - 11.0f, PelvisX + 11.0f, ShoulderY - 2.0f, PelvisY + 6.0f, Primary);
			Canvas.FillTaperedQuad(ShoulderX - 11.0f, ShoulderX - 2.0f, PelvisX - 11.0f, PelvisX - 3.0f, ShoulderY - 2.0f, PelvisY + 6.0f, PrimaryShade);
		}

		// Head skull, neck & stylized Limbus hair silhouette
		if (bLinWan)
		{
			Canvas.FillTaperedQuad(HeadX - 16.0f, HeadX - 4.0f, HeadX - 20.0f, HeadX - 6.0f, HeadY - 4.0f, ShoulderY + 24.0f, Hair);
		}
		Canvas.FillSegment(ShoulderX, ShoulderY, HeadX, HeadY + 9.0f, 4.0f, SkinShade);
		Canvas.FillEllipse(HeadX, HeadY, 13.0f, 14.5f, Skin);
		Canvas.FillEllipse(HeadX - 6.0f, HeadY + 3.0f, 5.0f, 8.5f, SkinShade);
		Canvas.FillEllipse(HeadX - 1.0f, HeadY - 8.5f, 14.8f, 10.5f, Hair);
		Canvas.FillTriangle(FVector2D(HeadX - 15.0f, HeadY - 6.0f), FVector2D(HeadX - 7.0f, HeadY - 14.0f), FVector2D(HeadX - 12.0f, HeadY + 8.0f), Hair);
		Canvas.FillTriangle(FVector2D(HeadX - 6.0f, HeadY - 12.0f), FVector2D(HeadX + 5.0f, HeadY - 11.0f), FVector2D(HeadX, HeadY - 2.0f), Hair);
		Canvas.FillTriangle(FVector2D(HeadX + 4.0f, HeadY - 12.0f), FVector2D(HeadX + 15.0f, HeadY - 6.0f), FVector2D(HeadX + 11.0f, HeadY + 2.0f), Hair);
		if (bFan)
		{
			Canvas.FillEllipse(HeadX - 1.5f, HeadY - 14.5f, 12.0f, 5.5f, Accent);
		}

		// Front arm
		const FColor FrontSleeve = bVolunteer ? Shirt : Primary;
		Canvas.FillSegment(ShoulderX - 4.0f, ShoulderY + 4.0f, ShoulderX + FrontHandOffset.X, ShoulderY + FrontHandOffset.Y, 4.5f, FrontSleeve);
		Canvas.FillCircle(ShoulderX + FrontHandOffset.X, ShoulderY + FrontHandOffset.Y, 4.4f, Skin);

		// Unified dark ink contour around complete character frame silhouette
		Canvas.AddInkOutline(2, Ink);

		// Crisp interior linework & outfit details over inked silhouette
		Canvas.FillTriangle(FVector2D(ShoulderX - 3.0f, ShoulderY - 1.0f), FVector2D(ShoulderX + 6.0f, ShoulderY - 1.0f), FVector2D(ShoulderX + 2.0f, ShoulderY + 12.0f), Shirt);
		if (!bVolunteer && !bCoordinator)
		{
			Canvas.FillRoundedBox(ShoulderX - 5.0f, ShoulderY - 3.0f, ShoulderX + 6.0f, ShoulderY + 3.0f, 1.5f, Accent);
			Canvas.FillSegment(PelvisX - 11.0f, PelvisY - 3.0f, PelvisX + 11.0f, PelvisY - 3.0f, 1.2f, Ink);
		}
		else if (bVolunteer)
		{
			Canvas.FillRoundedBox(ShoulderX + 2.0f, ShoulderY + 6.0f, ShoulderX + 8.0f, ShoulderY + 12.0f, 1.0f, Accent);
		}
		else
		{
			Canvas.FillSegment(ShoulderX + 2.0f, ShoulderY + 1.0f, ShoulderX + 2.0f, ShoulderY + 11.0f, 1.3f, Accent);
		}
		Canvas.FillSegment(HeadX - 6.0f, HeadY - 12.0f, HeadX + 1.0f, HeadY - 13.0f, 1.0f, HairSheen);

		// Expressive eyes, brows & mouth based on state + idle blink on frame 3
		const float EyeY = HeadY + 1.5f;
		const float LeftEyeX = HeadX - 2.0f;
		const float RightEyeX = HeadX + 6.8f;

		if (bShock)
		{
			Canvas.FillCircle(LeftEyeX, EyeY, 2.5f, FColor(250, 252, 255, 255));
			Canvas.FillCircle(RightEyeX, EyeY, 2.5f, FColor(250, 252, 255, 255));
			Canvas.FillCircle(LeftEyeX + 0.5f, EyeY, 1.1f, Ink);
			Canvas.FillCircle(RightEyeX + 0.5f, EyeY, 1.1f, Ink);
			Canvas.FillSegment(LeftEyeX - 2.5f, EyeY - 4.5f, LeftEyeX + 1.5f, EyeY - 5.5f, 1.0f, Ink);
			Canvas.FillSegment(RightEyeX - 1.5f, EyeY - 5.5f, RightEyeX + 2.5f, EyeY - 4.5f, 1.0f, Ink);
			Canvas.FillEllipse(HeadX + 2.8f, HeadY + 9.0f, 2.2f, 2.8f, Ink);
		}
		else if (bArgue)
		{
			Canvas.FillEllipse(LeftEyeX, EyeY + 0.5f, 2.2f, 1.7f, Ink);
			Canvas.FillEllipse(RightEyeX, EyeY + 0.5f, 2.2f, 1.7f, Ink);
			Canvas.FillSegment(LeftEyeX - 2.5f, EyeY - 4.0f, LeftEyeX + 2.5f, EyeY - 1.8f, 1.1f, Ink);
			Canvas.FillSegment(RightEyeX + 2.5f, EyeY - 4.0f, RightEyeX - 2.5f, EyeY - 1.8f, 1.1f, Ink);
			Canvas.FillEllipse(HeadX + 3.0f, HeadY + 8.8f, 2.8f, 1.8f, Ink);
		}
		else if (bHandoff || bWarm)
		{
			Canvas.FillSegment(LeftEyeX - 2.2f, EyeY + 0.5f, LeftEyeX, EyeY - 1.5f, 1.1f, Ink);
			Canvas.FillSegment(LeftEyeX, EyeY - 1.5f, LeftEyeX + 2.2f, EyeY + 0.5f, 1.1f, Ink);
			Canvas.FillSegment(RightEyeX - 2.2f, EyeY + 0.5f, RightEyeX, EyeY - 1.5f, 1.1f, Ink);
			Canvas.FillSegment(RightEyeX, EyeY - 1.5f, RightEyeX + 2.2f, EyeY + 0.5f, 1.1f, Ink);
			Canvas.FillSegment(HeadX + 0.5f, HeadY + 8.0f, HeadX + 3.0f, HeadY + 9.6f, 1.0f, Ink);
			Canvas.FillSegment(HeadX + 3.0f, HeadY + 9.6f, HeadX + 5.8f, HeadY + 8.0f, 1.0f, Ink);
		}
		else if (!bWalk && !bPickup && !bCare && Frame == 3)
		{
			// Natural idle blink on frame 3
			Canvas.FillSegment(LeftEyeX - 2.2f, EyeY + 0.5f, LeftEyeX + 2.0f, EyeY + 0.5f, 1.0f, Ink);
			Canvas.FillSegment(RightEyeX - 2.2f, EyeY + 0.5f, RightEyeX + 2.0f, EyeY + 0.5f, 1.0f, Ink);
			Canvas.FillSegment(HeadX + 1.0f, HeadY + 8.5f, HeadX + 4.8f, HeadY + 8.5f, 0.9f, Ink);
		}
		else
		{
			Canvas.FillEllipse(LeftEyeX, EyeY, 2.0f, 2.3f, Ink);
			Canvas.FillEllipse(RightEyeX, EyeY, 2.0f, 2.3f, Ink);
			Canvas.FillCircle(LeftEyeX + 0.5f, EyeY - 0.6f, 0.7f, FColor(250, 252, 255, 255));
			Canvas.FillCircle(RightEyeX + 0.5f, EyeY - 0.6f, 0.7f, FColor(250, 252, 255, 255));
			Canvas.FillSegment(HeadX + 1.0f, HeadY + 8.5f, HeadX + 4.8f, HeadY + 8.5f, 0.9f, Ink);
		}
	}

	UTexture2D* GetOrCreateSequenceFrameTexture(const FName& SequenceName, int32 FrameIndex)
	{
		const int32 ClampedFrame = FMath::Clamp(FrameIndex, 0, 3);
		const FName CacheKey(*FString::Printf(TEXT("Seq_%s_%d"), *SequenceName.ToString(), ClampedFrame));

		static TMap<FName, TWeakObjectPtr<UTexture2D>> CachedSequenceTextures;
		if (TWeakObjectPtr<UTexture2D>* Existing = CachedSequenceTextures.Find(CacheKey))
		{
			if (Existing->IsValid())
			{
				return Existing->Get();
			}
		}

		const FString SeqStr = SequenceName.ToString();
		FString CustomCharFile;
		if (SeqStr.StartsWith(TEXT("LinWan_")))
		{
			CustomCharFile = TEXT("char_linwan.jpg");
		}
		else if (SeqStr.StartsWith(TEXT("XiaoBei_")))
		{
			CustomCharFile = TEXT("char_xiaobei.jpg");
		}
		else if (SeqStr.StartsWith(TEXT("Fan_")))
		{
			CustomCharFile = TEXT("char_fan.jpg");
		}

		if (!CustomCharFile.IsEmpty())
		{
			if (UTexture2D* CustomChar = LoadCustomArtTexture(CustomCharFile, true))
			{
				CachedSequenceTextures.Add(CacheKey, CustomChar);
				return CustomChar;
			}
		}

		FBackstagePixelCanvas Canvas;
		DrawCharacterSequenceFrame(Canvas, SequenceName, ClampedFrame);
		UTexture2D* Texture = CreateTextureFromCanvas(Canvas);
		if (Texture)
		{
			CachedSequenceTextures.Add(CacheKey, Texture);
		}
		return Texture;
	}

	UMaterialInterface* LoadSpriteMaterial(bool bLit)
	{
		static TWeakObjectPtr<UMaterialInterface> CachedLitMaterial;
		static TWeakObjectPtr<UMaterialInterface> CachedUnlitMaterial;
		TWeakObjectPtr<UMaterialInterface>& Slot = bLit ? CachedLitMaterial : CachedUnlitMaterial;
		if (Slot.IsValid())
		{
			return Slot.Get();
		}

		const TCHAR* Paths[] = {
			bLit ? TEXT("/Paper2D/MaskedLitSpriteMaterial.MaskedLitSpriteMaterial") : TEXT("/Paper2D/TranslucentUnlitSpriteMaterial.TranslucentUnlitSpriteMaterial"),
			bLit ? TEXT("/Paper2D/DefaultLitSpriteMaterial.DefaultLitSpriteMaterial") : TEXT("/Paper2D/MaskedUnlitSpriteMaterial.MaskedUnlitSpriteMaterial"),
			TEXT("/Paper2D/MaskedUnlitSpriteMaterial.MaskedUnlitSpriteMaterial")
		};
		for (const TCHAR* Path : Paths)
		{
			if (UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, Path))
			{
				Slot = Material;
				return Material;
			}
		}
		return nullptr;
	}

	UPaperSprite* GetOrCreateCachedRuntimeSprite(
		const FName& CacheKey,
		UTexture2D* Texture,
		UMaterialInterface* Material,
		const FVector2D& SizeUU,
		const FVector2D& PivotNormalized)
	{
		static TMap<FName, TWeakObjectPtr<UPaperSprite>> CachedSprites;
		if (TWeakObjectPtr<UPaperSprite>* Existing = CachedSprites.Find(CacheKey))
		{
			if (Existing->IsValid())
			{
				return Existing->Get();
			}
		}

		FBackstagePaperSpriteAccessor* Sprite = NewObject<FBackstagePaperSpriteAccessor>(GetTransientPackage());
		Sprite->AddToRoot();
		Sprite->InitRuntimeSprite(Texture, Material, SizeUU, PivotNormalized);
		CachedSprites.Add(CacheKey, Sprite);
		return Sprite;
	}

	void ApplySpriteToComponent(UPaperSpriteComponent* Component, UPaperSprite* Sprite, UMaterialInterface* Material, FLinearColor Color, bool bCastShadow)
	{
		if (!Component || !Sprite)
		{
			return;
		}
		if (Component->GetCollisionEnabled() != ECollisionEnabled::NoCollision)
		{
			Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
		if (Component->CastShadow != bCastShadow || Component->bCastDynamicShadow != bCastShadow || Component->bCastShadowAsTwoSided != bCastShadow)
		{
			Component->SetCastShadow(bCastShadow);
			Component->bCastDynamicShadow = bCastShadow;
			Component->bCastShadowAsTwoSided = bCastShadow;
		}
		if (Material && Component->GetMaterial(0) != Material)
		{
			Component->SetMaterial(0, Material);
		}
		if (Component->GetSprite() != Sprite)
		{
			Component->SetSprite(Sprite);
		}
		if (!Component->GetSpriteColor().Equals(Color, 0.001f))
		{
			Component->SetSpriteColor(Color);
		}
	}

	void BackstageSetupPart(UPaperSpriteComponent* Component, const FName& PartKind, const FVector2D& SizeUU, const FVector2D& PivotNormalized, FLinearColor Color, bool bLit, bool bCastShadow)
	{
		if (!Component)
		{
			return;
		}

		UMaterialInterface* Material = LoadSpriteMaterial(bLit);
		UTexture2D* Texture = GetOrCreatePartTexture(PartKind);
		const FName SpriteKey(*FString::Printf(
			TEXT("Part_%s_%dx%d_%dx%d_%d"),
			*PartKind.ToString(),
			FMath::RoundToInt(SizeUU.X),
			FMath::RoundToInt(SizeUU.Y),
			FMath::RoundToInt(PivotNormalized.X * 100.0),
			FMath::RoundToInt(PivotNormalized.Y * 100.0),
			bLit ? 1 : 0));
		UPaperSprite* Sprite = GetOrCreateCachedRuntimeSprite(SpriteKey, Texture, Material, SizeUU, PivotNormalized);
		ApplySpriteToComponent(Component, Sprite, Material, Color, bCastShadow);
	}

	void BackstageSetSpriteFrame(UPaperSpriteComponent* Component, const FName& SequenceName, int32 FrameIndex, const FVector2D& SizeUU, const FVector2D& PivotNormalized, FLinearColor Color, bool bLit, bool bCastShadow)
	{
		if (!Component)
		{
			return;
		}

		const int32 ClampedFrame = FMath::Clamp(FrameIndex, 0, 3);
		UMaterialInterface* Material = LoadSpriteMaterial(bLit);

		static TMap<FName, TWeakObjectPtr<UPaperSprite>> CachedAssetSprites;
		static TSet<FName> MissingAssetSprites;
		const FName AssetKey(*FString::Printf(TEXT("%s_%d"), *SequenceName.ToString(), ClampedFrame));

		UPaperSprite* Sprite = nullptr;
		if (TWeakObjectPtr<UPaperSprite>* Existing = CachedAssetSprites.Find(AssetKey))
		{
			if (Existing->IsValid())
			{
				Sprite = Existing->Get();
			}
		}

		if (!Sprite && !MissingAssetSprites.Contains(AssetKey))
		{
			const FString PackagePath = FString::Printf(TEXT("/Game/Sprites/%s_%d"), *SequenceName.ToString(), ClampedFrame);
			if (FPackageName::DoesPackageExist(PackagePath))
			{
				const FString ObjectPath = FString::Printf(TEXT("%s.%s_%d"), *PackagePath, *SequenceName.ToString(), ClampedFrame);
				Sprite = LoadObject<UPaperSprite>(nullptr, *ObjectPath);
				if (Sprite)
				{
					CachedAssetSprites.Add(AssetKey, Sprite);
				}
				else
				{
					MissingAssetSprites.Add(AssetKey);
				}
			}
			else
			{
				MissingAssetSprites.Add(AssetKey);
			}
		}

		if (Sprite)
		{
			ApplySpriteToComponent(Component, Sprite, Material, FLinearColor::White, bCastShadow);
			return;
		}

		UTexture2D* Texture = GetOrCreateSequenceFrameTexture(SequenceName, ClampedFrame);
		const FName SpriteKey(*FString::Printf(
			TEXT("Frame_%s_%d_%dx%d_%dx%d_%d"),
			*SequenceName.ToString(),
			ClampedFrame,
			FMath::RoundToInt(SizeUU.X),
			FMath::RoundToInt(SizeUU.Y),
			FMath::RoundToInt(PivotNormalized.X * 100.0),
			FMath::RoundToInt(PivotNormalized.Y * 100.0),
			bLit ? 1 : 0));
		Sprite = GetOrCreateCachedRuntimeSprite(SpriteKey, Texture, Material, SizeUU, PivotNormalized);
		// Procedural frame textures already bake the character's role outfit colors, so soften Color into a subtle tint.
		const FLinearColor SubtleTint = FMath::Lerp(FLinearColor::White, Color, 0.16f);
		ApplySpriteToComponent(Component, Sprite, Material, SubtleTint, bCastShadow);
	}

	void BackstagePrewarmSprites(APlayerController* Controller)
	{
		if (IsRunningCommandlet() || !FApp::CanEverRender())
		{
			return;
		}

		static bool bPrewarmed = false;
		if (bPrewarmed)
		{
			return;
		}
		bPrewarmed = true;

		const TCHAR* PartKinds[] = {
			TEXT("Shadow"),
			TEXT("Parcel"),
			TEXT("Umbrella"),
			TEXT("UmbrellaFolded"),
			TEXT("FanSign"),
			TEXT("RainStreak"),
			TEXT("BrandBanner"),
			TEXT("Backdrop_Rain"),
			TEXT("Backdrop_Charity"),
			TEXT("Emotion_Shock"),
			TEXT("Emotion_Anger"),
			TEXT("Emotion_Care"),
			TEXT("Emotion_Warm")
		};
		for (const TCHAR* Part : PartKinds)
		{
			GetOrCreatePartTexture(FName(Part));
		}

		UMaterialInterface* LitMaterial = LoadSpriteMaterial(true);
		const TCHAR* Roles[] = { TEXT("Artist"), TEXT("Volunteer"), TEXT("Coordinator"), TEXT("LinWan"), TEXT("XiaoBei"), TEXT("Fan") };
		const TCHAR* States[] = { TEXT("Idle"), TEXT("Walk") };
		const FVector2D BodySize(104.0, 156.0);
		const FVector2D BodyPivot(0.5, 1.0);

		for (const TCHAR* Role : Roles)
		{
			for (const TCHAR* State : States)
			{
				const FName SeqName(*FString::Printf(TEXT("%s_%s"), Role, State));
				UTexture2D* Texture = GetOrCreateSequenceFrameTexture(SeqName, 0);
				const FName SpriteKey(*FString::Printf(TEXT("Frame_%s_0_104x156_50x100_1"), *SeqName.ToString()));
				GetOrCreateCachedRuntimeSprite(SpriteKey, Texture, LitMaterial, BodySize, BodyPivot);
			}
		}
	}

	void BackstageSetupStageMesh(UStaticMeshComponent* Component, const FString& MeshPath, FLinearColor Color, float Roughness, float Metallic, bool bCastShadow)
	{
		if (!Component)
		{
			return;
		}

		UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *MeshPath);
		if (Mesh)
		{
			Component->SetStaticMesh(Mesh);
		}
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Component->SetCastShadow(bCastShadow);

		UMaterialInterface* BaseMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/LevelPrototyping/Materials/M_FlatCol.M_FlatCol"));
		if (!BaseMaterial)
		{
			BaseMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
		}
		if (BaseMaterial)
		{
			UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(BaseMaterial, Component);
			if (MID)
			{
				MID->SetVectorParameterValue(FName(TEXT("BaseColor")), Color);
				MID->SetVectorParameterValue(FName(TEXT("Color")), Color);
				MID->SetScalarParameterValue(FName(TEXT("Roughness")), Roughness);
				MID->SetScalarParameterValue(FName(TEXT("Metallic")), Metallic);
				Component->SetMaterial(0, MID);
			}
		}
	}

	FVector BackstageStageToWorld(const FVector2D& StagePoint, float HeightUU)
	{
		return FVector(-StagePoint.Y * 100.0, StagePoint.X * 100.0, HeightUU);
	}

	float BackstageStationYaw(int32 Station)
	{
		const float AngleRad = (Station == 0) ? 0.0f : ((Station == 1) ? -0.63f : 0.63f);
		return FMath::RadiansToDegrees(AngleRad);
	}

	void BackstageComputeCameraTransform(float AngleRad, float Center, float Pan, float Tilt, float Zoom, FVector& OutLocation, FRotator& OutRotation, float& OutFOV)
	{
		const float YawDeg = FMath::RadiansToDegrees(AngleRad);
		const FVector Forward(FMath::Cos(AngleRad), FMath::Sin(AngleRad), 0.0);
		const FVector Right(-FMath::Sin(AngleRad), FMath::Cos(AngleRad), 0.0);
		const double LateralShift = static_cast<double>(Center + Pan) * 100.0;
		// Crane vertical movement + wider upward/downward pitch for better framing freedom.
		const double VerticalShift = static_cast<double>(Tilt) * 44.0;
		const double DistancePull = static_cast<double>(BackstageCameraDistance) - FMath::Abs(static_cast<double>(Tilt)) * 18.0;

		OutLocation = Right * LateralShift - Forward * DistancePull + FVector(0.0, 0.0, 128.0 + VerticalShift);
		OutRotation = FRotator(-2.5 + static_cast<double>(Tilt) * 10.5, YawDeg, 0.0);

		const float TanHalfFOV = (100.0f / BackstageCameraDistance) / (2.0f * BackstageProjectionScale * FMath::Max(Zoom, 0.5f));
		OutFOV = FMath::Clamp(FMath::RadiansToDegrees(2.0f * FMath::Atan(TanHalfFOV)), 18.0f, 90.0f);
	}

	TArray<TWeakObjectPtr<UTextureRenderTarget2D>>& GetCapturedPhotoTargets()
	{
		static TArray<TWeakObjectPtr<UTextureRenderTarget2D>> Targets;
		if (Targets.Num() < BackstageMaxCaptureSlots)
		{
			Targets.SetNum(BackstageMaxCaptureSlots);
		}
		return Targets;
	}

	UTextureRenderTarget2D* GetOrCreateCaptureTarget(UObject* Outer, int32 PhotoIndex)
	{
		if (PhotoIndex < 0 || PhotoIndex >= BackstageMaxCaptureSlots)
		{
			return nullptr;
		}
		TArray<TWeakObjectPtr<UTextureRenderTarget2D>>& Targets = GetCapturedPhotoTargets();
		if (Targets[PhotoIndex].IsValid())
		{
			return Targets[PhotoIndex].Get();
		}
		UTextureRenderTarget2D* RT = NewObject<UTextureRenderTarget2D>(Outer ? Outer : GetTransientPackage());
		RT->RenderTargetFormat = RTF_RGBA8_SRGB;
		RT->ClearColor = FLinearColor(0.026f, 0.045f, 0.068f, 1.0f);
		RT->InitAutoFormat(512, 288);
		RT->UpdateResourceImmediate(true);
		Targets[PhotoIndex] = RT;
		return RT;
	}

	bool BackstagePressed(const APlayerController* Controller, const FName& KeyName)
	{
		return Controller->WasInputKeyJustPressed(FKey(KeyName));
	}

	bool BackstageHeld(const APlayerController* Controller, const FName& KeyName)
	{
		return Controller->IsInputKeyDown(FKey(KeyName));
	}

	void SetBackstagePointer(APlayerController* Controller, bool bVisible)
	{
		Controller->SetShowMouseCursor(bVisible);
		if (bVisible)
		{
			FInputModeGameAndUI Mode;
			Mode.SetHideCursorDuringCapture(false);
			Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			Controller->SetInputMode(Mode);
		}
		else
		{
			Controller->SetInputMode(FInputModeGameOnly());
		}
	}

	FVector2D BackstagePointer(const APlayerController* Controller)
	{
		float X, Y;
		int32 Width, Height;
		Controller->GetViewportSize(Width, Height);
		if (Width <= 0 || Height <= 0 || !Controller->GetMousePosition(X, Y))
		{
			return FVector2D(-1, -1);
		}

		const double Scale = FMath::Min(Width / 1280.0, Height / 720.0);
		const FVector2D Origin((Width - 1280 * Scale) * 0.5, (Height - 720 * Scale) * 0.5);
		return (FVector2D(X, Y) - Origin) / Scale;
	}

	FVector BackstageWorldPoint(const APlayerController* Controller, const FVector2D& StagePoint, float HeightUU)
	{
		return BackstageStageToWorld(StagePoint, HeightUU);
	}

	float BackstageCameraYaw(const APlayerController* Controller, int32 Station)
	{
		return BackstageStationYaw(Station);
	}

	FVector2D BackstageMouseDelta(APlayerController* Controller)
	{
		if (!Controller)
		{
			return FVector2D::ZeroVector;
		}

		float RawDx = 0.0f;
		float RawDy = 0.0f;
		Controller->GetInputMouseDelta(RawDx, RawDy);

		int32 Width = 0;
		int32 Height = 0;
		Controller->GetViewportSize(Width, Height);
		float MouseX = 0.0f;
		float MouseY = 0.0f;
		const bool bHasMousePos = (Width > 0 && Height > 0 && Controller->GetMousePosition(MouseX, MouseY));

		static TWeakObjectPtr<APlayerController> LastController;
		static FVector2D LastMousePos = FVector2D::ZeroVector;
		static bool bHasLastMousePos = false;

		FVector2D PosDelta = FVector2D::ZeroVector;
		if (bHasMousePos)
		{
			if (LastController.Get() == Controller && bHasLastMousePos)
			{
				PosDelta = FVector2D(MouseX - LastMousePos.X, MouseY - LastMousePos.Y);
			}
			LastController = Controller;
			LastMousePos = FVector2D(MouseX, MouseY);
			bHasLastMousePos = true;
		}
		else
		{
			bHasLastMousePos = false;
		}

		if (!FMath::IsNearlyZero(RawDx) || !FMath::IsNearlyZero(RawDy))
		{
			return FVector2D(
				FMath::Clamp(static_cast<double>(RawDx) * 0.045, -0.35, 0.35),
				FMath::Clamp(static_cast<double>(RawDy) * 0.045, -0.35, 0.35));
		}

		if (Width > 0 && Height > 0 && (FMath::Abs(PosDelta.X) < 250.0 && FMath::Abs(PosDelta.Y) < 250.0))
		{
			const double Scale = FMath::Max(1.0, FMath::Min(Width / 1280.0, Height / 720.0));
			return FVector2D(
				FMath::Clamp(PosDelta.X / (Scale * 180.0), -0.35, 0.35),
				FMath::Clamp(-PosDelta.Y / (Scale * 180.0), -0.35, 0.35));
		}
		return FVector2D::ZeroVector;
	}

	float BackstageWheelDelta(const APlayerController* Controller)
	{
		if (!Controller)
		{
			return 0.0f;
		}
		float Wheel = 0.0f;
		if (Controller->WasInputKeyJustPressed(EKeys::MouseScrollUp))
		{
			Wheel += 1.0f;
		}
		if (Controller->WasInputKeyJustPressed(EKeys::MouseScrollDown))
		{
			Wheel -= 1.0f;
		}
		return Wheel;
	}

	void BackstageSyncCamera(APlayerController* Controller, AActor* CameraHost, float AngleRad, float Center, float Pan, float Tilt, float Zoom, bool bActivateViewTarget)
	{
		if (!Controller || !CameraHost)
		{
			return;
		}

		UCameraComponent* Camera = CameraHost->FindComponentByClass<UCameraComponent>();
		if (!Camera)
		{
			Camera = NewObject<UCameraComponent>(CameraHost, TEXT("BackstageStageCamera"));
			if (CameraHost->GetRootComponent())
			{
				Camera->SetupAttachment(CameraHost->GetRootComponent());
			}
			Camera->RegisterComponent();
		}

		FVector Location;
		FRotator Rotation;
		float FOV = 50.0f;
		BackstageComputeCameraTransform(AngleRad, Center, Pan, Tilt, Zoom, Location, Rotation, FOV);

		Camera->bConstrainAspectRatio = false;
		Camera->SetFieldOfView(FOV);
		Camera->SetWorldLocationAndRotation(Location, Rotation);

		if (bActivateViewTarget && Controller->GetViewTarget() != CameraHost)
		{
			Controller->SetViewTarget(CameraHost);
		}
	}

	void BackstageCapturePhoto(APlayerController* Controller, AActor* CameraHost, int32 PhotoIndex, float AngleRad, float Center, float Pan, float Tilt, float Zoom)
	{
		if (IsRunningCommandlet() || !FApp::CanEverRender() || !CameraHost)
		{
			return;
		}

		UTextureRenderTarget2D* RT = GetOrCreateCaptureTarget(CameraHost, PhotoIndex);
		if (!RT)
		{
			return;
		}

		USceneCaptureComponent2D* Capture = CameraHost->FindComponentByClass<USceneCaptureComponent2D>();
		if (!Capture)
		{
			Capture = NewObject<USceneCaptureComponent2D>(CameraHost, TEXT("BackstagePhotoCapture"));
			if (CameraHost->GetRootComponent())
			{
				Capture->SetupAttachment(CameraHost->GetRootComponent());
			}
			Capture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
			Capture->bCaptureEveryFrame = false;
			Capture->bCaptureOnMovement = false;
			Capture->bAlwaysPersistRenderingState = true;
			Capture->ShowFlags.SetMotionBlur(false);
			Capture->ShowFlags.SetAmbientOcclusion(false);
			Capture->ShowFlags.SetVolumetricFog(false);
			Capture->RegisterComponent();
		}

		FVector Location;
		FRotator Rotation;
		float FOV = 50.0f;
		BackstageComputeCameraTransform(AngleRad, Center, Pan, Tilt, Zoom, Location, Rotation, FOV);

		Capture->TextureTarget = RT;
		Capture->FOVAngle = FOV;
		Capture->SetWorldLocationAndRotation(Location, Rotation);
		Capture->CaptureScene();
	}

	void BackstageClearPhotos(APlayerController* Controller)
	{
		TArray<TWeakObjectPtr<UTextureRenderTarget2D>>& Targets = GetCapturedPhotoTargets();
		for (TWeakObjectPtr<UTextureRenderTarget2D>& Entry : Targets)
		{
			Entry.Reset();
		}
	}

	bool BackstageIsMainMenuMap(const APlayerController* Controller)
	{
		if (!Controller || !Controller->GetWorld())
		{
			return false;
		}
		return Controller->GetWorld()->GetMapName().Contains(TEXT("MainMenu"), ESearchCase::IgnoreCase);
	}

	void BackstageOpenMap(APlayerController* Controller, const FName& MapName)
	{
		if (Controller && Controller->GetWorld())
		{
			UGameplayStatics::OpenLevel(Controller->GetWorld(), MapName);
		}
	}

	void BackstageSetLevel(const APlayerController* Controller, int32 LevelIndex)
	{
		GBackstageSelectedLevel = FMath::Clamp(LevelIndex, 0, 1);
	}

	int32 BackstageGetLevel(const APlayerController* Controller)
	{
		return GBackstageSelectedLevel;
	}

	void BackstageQuitGame(APlayerController* Controller)
	{
		if (Controller)
		{
			Controller->ConsoleCommand(TEXT("quit"));
		}
	}

	AActor* BackstageFindStageActor(const APlayerController* Controller)
	{
		if (!Controller || !Controller->GetWorld())
		{
			return nullptr;
		}
		TArray<AActor*> Actors;
		UGameplayStatics::GetAllActorsOfClass(Controller->GetWorld(), AActor::StaticClass(), Actors);
		for (AActor* Actor : Actors)
		{
			if (Actor && Actor->GetClass()->GetName().Contains(TEXT("BackstageStageActor")))
			{
				return Actor;
			}
		}
		return nullptr;
	}

	bool BackstageDrawPhoto(AHUD* HUD, int32 PhotoIndex, float X, float Y, float W, float H, float OriginalPan, float OriginalZoom, float CropPan, float CropZoom)
	{
		if (!HUD || PhotoIndex < 0 || PhotoIndex >= BackstageMaxCaptureSlots)
		{
			return false;
		}
		TArray<TWeakObjectPtr<UTextureRenderTarget2D>>& Targets = GetCapturedPhotoTargets();
		UTextureRenderTarget2D* RT = Targets[PhotoIndex].Get();
		if (!RT || !RT->GetResource())
		{
			return false;
		}

		const float SafeOriginalZoom = FMath::Max(OriginalZoom, 0.001f);
		const float SafeCropZoom = FMath::Max(CropZoom, SafeOriginalZoom);
		const float Ratio = FMath::Clamp(SafeOriginalZoom / SafeCropZoom, 0.1f, 1.0f);
		const float Travel = (0.5f / BackstageProjectionScale) * (1.0f / SafeOriginalZoom - 1.0f / SafeCropZoom);
		const float NormalizedPan = (Travel > KINDA_SMALL_NUMBER) ? FMath::Clamp((CropPan - OriginalPan) / Travel, -1.0f, 1.0f) : 0.0f;
		const float U = FMath::Clamp((1.0f - Ratio) * 0.5f * (1.0f + NormalizedPan), 0.0f, 1.0f - Ratio);
		const float V = (1.0f - Ratio) * 0.5f;

		HUD->DrawTexture(RT, X, Y, W, H, U, V, Ratio, Ratio, FLinearColor::White, BLEND_Opaque, 1.0f, false, 0.0f, FVector2D::ZeroVector);
		return true;
	}

	void BackstageSetTransform(AActor* Actor, const FVector& Location, const FRotator& Rotation)
	{
		if (Actor)
		{
			Actor->SetActorLocationAndRotation(Location, Rotation, false, nullptr, ETeleportType::TeleportPhysics);
		}
	}

	void BackstageSetScale(USceneComponent* Component, const FVector& Scale3D)
	{
		if (Component)
		{
			Component->SetRelativeScale3D(Scale3D);
		}
	}

	void BackstageSetVisible(USceneComponent* Component, bool bVisible)
	{
		if (Component)
		{
			Component->SetVisibility(bVisible, false);
		}
	}

	void BackstageAttachTo(USceneComponent* Component, USceneComponent* Parent)
	{
		if (Component && Parent)
		{
			Component->AttachToComponent(Parent, FAttachmentTransformRules::KeepRelativeTransform);
		}
	}

	void ConfigureLocalLight(ULocalLightComponent* Light, const FVector& RelativeLocation, FLinearColor Color, float Intensity, float AttenuationRadius, bool bCastShadow)
	{
		Light->SetRelativeLocation(RelativeLocation);
		Light->SetIntensityUnits(ELightUnits::Candelas);
		Light->SetIntensity(Intensity);
		Light->SetLightColor(Color);
		Light->SetAttenuationRadius(AttenuationRadius);
		Light->SetCastShadows(bCastShadow);
	}

	void BackstageSetupSpotLight(USpotLightComponent* Light, const FVector& RelativeLocation, const FRotator& RelativeRotation, FLinearColor Color, float Intensity, float AttenuationRadius, float InnerConeAngle, float OuterConeAngle, bool bCastShadow)
	{
		if (!Light)
		{
			return;
		}
		ConfigureLocalLight(Light, RelativeLocation, Color, Intensity, AttenuationRadius, bCastShadow);
		Light->SetRelativeRotation(RelativeRotation);
		Light->SetInnerConeAngle(InnerConeAngle);
		Light->SetOuterConeAngle(OuterConeAngle);
	}

	void BackstageSetupPointLight(UPointLightComponent* Light, const FVector& RelativeLocation, FLinearColor Color, float Intensity, float AttenuationRadius, bool bCastShadow)
	{
		if (!Light)
		{
			return;
		}
		ConfigureLocalLight(Light, RelativeLocation, Color, Intensity, AttenuationRadius, bCastShadow);
	}

	void BackstageSetupDirectionalLight(UDirectionalLightComponent* Light, const FRotator& RelativeRotation, FLinearColor Color, float Intensity, bool bCastShadow)
	{
		if (!Light)
		{
			return;
		}
		Light->SetRelativeRotation(RelativeRotation);
		Light->SetIntensity(Intensity);
		Light->SetLightColor(Color);
		Light->SetCastShadows(bCastShadow);
	}

	void BackstageSetupFog(UExponentialHeightFogComponent* Fog, float Density, FLinearColor InscatteringColor, bool bVolumetric)
	{
		if (!Fog)
		{
			return;
		}
		Fog->SetFogDensity(Density);
		Fog->SetFogInscatteringColor(InscatteringColor);
		Fog->SetVolumetricFog(bVolumetric);
	}

	struct FBackstageHudAccessor : public AHUD
	{
		UCanvas* GetProtectedCanvas() const
		{
			return Canvas.Get();
		}
	};

	void BackstageDrawText(AHUD* HUD, const FString& Text, FLinearColor Color, float X, float Y, UFont* Font, float Scale, bool bScalePosition)
	{
		UCanvas* HudCanvas = HUD ? static_cast<FBackstageHudAccessor*>(HUD)->GetProtectedCanvas() : nullptr;
		if (!HudCanvas || Text.IsEmpty())
		{
			return;
		}
		const int32 FontPx = FMath::Clamp(FMath::RoundToInt(14.0f * FMath::Max(Scale, 0.25f)), 8, 96);
		const FSlateFontInfo FontInfo = FCoreStyle::GetDefaultFontStyle("Regular", FontPx);
		FCanvasTextItem TextItem(FVector2D(X, Y), FText::FromString(Text), FontInfo, Color);
		TextItem.Scale = FVector2D(1.0, 1.0);
		HudCanvas->DrawItem(TextItem);
	}

	float BackstageMeasureTextWidth(const AHUD* HUD, const FString& Text, float Scale)
	{
		if (Text.IsEmpty() || !FSlateApplication::IsInitialized())
		{
			return 0.0f;
		}
		const int32 FontPx = FMath::Clamp(FMath::RoundToInt(14.0f * FMath::Max(Scale, 0.25f)), 8, 96);
		const FSlateFontInfo FontInfo = FCoreStyle::GetDefaultFontStyle("Regular", FontPx);
		const TSharedRef<FSlateFontMeasure> FontMeasure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
		return static_cast<float>(FontMeasure->Measure(Text, FontInfo).X);
	}

	FVector2D BackstageProjectWorld(const APlayerController* Controller, const FVector& WorldPos)
	{
		if (!Controller)
		{
			return FVector2D(-9999.0, -9999.0);
		}
		int32 Width = 0;
		int32 Height = 0;
		Controller->GetViewportSize(Width, Height);
		if (Width <= 0 || Height <= 0)
		{
			return FVector2D(-9999.0, -9999.0);
		}
		FVector2D ScreenPos = FVector2D::ZeroVector;
		if (!Controller->ProjectWorldLocationToScreen(WorldPos, ScreenPos, true))
		{
			return FVector2D(-9999.0, -9999.0);
		}
		const double ViewScale = FMath::Min(Width / 1280.0, Height / 720.0);
		const FVector2D Origin((Width - 1280.0 * ViewScale) * 0.5, (Height - 720.0 * ViewScale) * 0.5);
		return (ScreenPos - Origin) / FMath::Max(ViewScale, 0.001);
	}
}

static const FAngelscriptBind BindBackstageControls(
	TEXT("MegaJamProject.BackstageControls"),
	EAngelscriptBindPhase::ExplicitBindings,
	[](FAngelscriptBinds& Binds)
	{
		auto Actor = Binds.ExistingClassForTarget("AActor");
		Actor.Method("void BackstageSetTransform(const FVector& Location, const FRotator& Rotation)", &BackstageSetTransform);

		auto SceneComp = Binds.ExistingClassForTarget("USceneComponent");
		SceneComp.Method("void BackstageSetScale(const FVector& Scale3D)", &BackstageSetScale);
		SceneComp.Method("void BackstageSetVisible(bool bVisible)", &BackstageSetVisible);
		SceneComp.Method("void BackstageAttachTo(USceneComponent Parent)", &BackstageAttachTo);

		auto SpotLight = Binds.ExistingClassForTarget("USpotLightComponent");
		SpotLight.Method("void BackstageSetupSpotLight(const FVector& RelativeLocation, const FRotator& RelativeRotation, FLinearColor Color, float32 Intensity, float32 AttenuationRadius, float32 InnerConeAngle, float32 OuterConeAngle, bool bCastShadow)", &BackstageSetupSpotLight);

		auto PointLight = Binds.ExistingClassForTarget("UPointLightComponent");
		PointLight.Method("void BackstageSetupPointLight(const FVector& RelativeLocation, FLinearColor Color, float32 Intensity, float32 AttenuationRadius, bool bCastShadow)", &BackstageSetupPointLight);

		auto DirLight = Binds.ExistingClassForTarget("UDirectionalLightComponent");
		DirLight.Method("void BackstageSetupDirectionalLight(const FRotator& RelativeRotation, FLinearColor Color, float32 Intensity, bool bCastShadow)", &BackstageSetupDirectionalLight);

		auto FogComp = Binds.ExistingClassForTarget("UExponentialHeightFogComponent");
		FogComp.Method("void BackstageSetupFog(float32 Density, FLinearColor InscatteringColor, bool bVolumetric)", &BackstageSetupFog);

		auto SpriteComp = Binds.ExistingClassForTarget("UPaperSpriteComponent");
		SpriteComp.Method("void BackstageSetupPart(const FName& PartKind, const FVector2D& SizeUU, const FVector2D& PivotNormalized, FLinearColor Color, bool bLit, bool bCastShadow)", &BackstageSetupPart);
		SpriteComp.Method("void BackstageSetSpriteFrame(const FName& SequenceName, int32 FrameIndex, const FVector2D& SizeUU, const FVector2D& PivotNormalized, FLinearColor Color, bool bLit, bool bCastShadow)", &BackstageSetSpriteFrame);

		auto MeshComp = Binds.ExistingClassForTarget("UStaticMeshComponent");
		MeshComp.Method("void BackstageSetupStageMesh(const FString& MeshPath, FLinearColor Color, float32 Roughness, float32 Metallic, bool bCastShadow)", &BackstageSetupStageMesh);

		auto Controller = Binds.ExistingClassForTarget("APlayerController");
		Controller.Method("bool BackstagePressed(const FName& KeyName) const", &BackstagePressed);
		Controller.Method("bool BackstageHeld(const FName& KeyName) const", &BackstageHeld);
		Controller.Method("void SetBackstagePointer(bool bVisible)", &SetBackstagePointer);
		Controller.Method("FVector2D BackstagePointer() const", &BackstagePointer);
		Controller.Method("FVector2D BackstageMouseDelta()", &BackstageMouseDelta);
		Controller.Method("float32 BackstageWheelDelta() const", &BackstageWheelDelta);
		Controller.Method("FVector BackstageWorldPoint(const FVector2D& StagePoint, float32 HeightUU) const", &BackstageWorldPoint);
		Controller.Method("float32 BackstageCameraYaw(int32 Station) const", &BackstageCameraYaw);
		Controller.Method("FVector2D BackstageProjectWorld(const FVector& WorldPos) const", &BackstageProjectWorld);
		Controller.Method("void BackstageSyncCamera(AActor CameraHost, float32 AngleRad, float32 Center, float32 Pan, float32 Tilt, float32 Zoom, bool bActivateViewTarget)", &BackstageSyncCamera);
		Controller.Method("void BackstageCapturePhoto(AActor CameraHost, int32 PhotoIndex, float32 AngleRad, float32 Center, float32 Pan, float32 Tilt, float32 Zoom)", &BackstageCapturePhoto);
		Controller.Method("void BackstageClearPhotos()", &BackstageClearPhotos);
		Controller.Method("void BackstagePrewarmSprites()", &BackstagePrewarmSprites);
		Controller.Method("bool BackstageIsMainMenuMap() const", &BackstageIsMainMenuMap);
		Controller.Method("void BackstageOpenMap(const FName& MapName)", &BackstageOpenMap);
		Controller.Method("void BackstageSetLevel(int32 LevelIndex)", &BackstageSetLevel);
		Controller.Method("int32 BackstageGetLevel() const", &BackstageGetLevel);
		Controller.Method("void BackstageQuitGame()", &BackstageQuitGame);
		Controller.Method("AActor BackstageFindStageActor() const", &BackstageFindStageActor);

		auto HUD = Binds.ExistingClassForTarget("AHUD");
		HUD.Method("APlayerController BackstagePlayer() const", METHOD_TRIVIAL(AHUD, GetOwningPlayerController));
		HUD.Method("void BackstageRect(FLinearColor Color, float32 X, float32 Y, float32 W, float32 H)", METHOD_TRIVIAL(AHUD, DrawRect));
		HUD.Method("void BackstageText(const FString& Text, FLinearColor Color, float32 X, float32 Y, UFont Font, float32 Scale, bool bScalePosition)", &BackstageDrawText);
		HUD.Method("float32 BackstageMeasureTextWidth(const FString& Text, float32 Scale) const", &BackstageMeasureTextWidth);
		HUD.Method("bool BackstageDrawPhoto(int32 PhotoIndex, float32 X, float32 Y, float32 W, float32 H, float32 OriginalPan, float32 OriginalZoom, float32 CropPan, float32 CropZoom)", &BackstageDrawPhoto);
	});
