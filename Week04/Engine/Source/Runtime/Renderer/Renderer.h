#pragma once
#pragma comment(lib, "user32")
#pragma comment(lib, "d3d11")
#pragma comment(lib, "d3dcompiler")

#define _TCHAR_DEFINED
#include <d3d11.h>
#include "EngineBaseTypes.h"
#include "Define.h"
#include "Container/Map.h"
#include "Container/Set.h"

class UStaticMesh;
class ULightComponentBase;
class UWorld;
class FGraphicsDevice;
class UMaterial;
struct FStaticMaterial;
class UObject;
class FEditorViewportClient;
class UBillboardComponent;
class UStaticMeshComponent;
class UGizmoBaseComponent;
class UPrimitiveComponent;
class FOctreeNode;

class FRenderer 
{

private:
    float litFlag = 0;
public:
    FGraphicsDevice* Graphics;
    ID3D11VertexShader* VertexShader = nullptr;
    ID3D11PixelShader* PixelShader = nullptr;
    ID3D11InputLayout* InputLayout = nullptr;
    ID3D11Buffer* ConstantBuffer = nullptr;

    ID3D11Buffer* ConstantBufferView = nullptr;
    ID3D11Buffer* ConstantBufferProjection = nullptr;
    
    ID3D11Buffer* LightingBuffer = nullptr;
    ID3D11Buffer* FlagBuffer = nullptr;
    ID3D11Buffer* MaterialConstantBuffer = nullptr;
    ID3D11Buffer* SubMeshConstantBuffer = nullptr;
    ID3D11Buffer* TextureConstantBuffer = nullptr;

    FLighting lightingData;

    uint32 Stride;
    uint32 Stride2;

public:
    void Initialize(FGraphicsDevice* graphics);
   
    void PrepareShader() const;
    void PrepareShaderDeferred(ID3D11DeviceContext* Context) const;
    
    //Render
    void RenderPrimitive(ID3D11Buffer* pBuffer, UINT numVertices) const;
    void RenderPrimitive(ID3D11Buffer* pVertexBuffer, UINT numVertices, ID3D11Buffer* pIndexBuffer, UINT numIndices) const;
    void RenderPrimitive(OBJ::FStaticMeshRenderData* renderData, TArray<FStaticMaterial*> materials, TArray<UMaterial*> overrideMaterial, int selectedSubMeshIndex) const;
   
    void RenderTexturedModelPrimitive(ID3D11Buffer* pVertexBuffer, UINT numVertices, ID3D11Buffer* pIndexBuffer, UINT numIndices, ID3D11ShaderResourceView* InTextureSRV, ID3D11SamplerState* InSamplerState) const;
    //Release
    void Release();
    void ReleaseShader();
    void ReleaseBuffer(ID3D11Buffer*& Buffer) const;
    void ReleaseConstantBuffer();

    void ResetVertexShader() const;
    void ResetPixelShader() const;
    void CreateShader();

    void SetVertexShader(const FWString& filename, const FString& funcname, const FString& version);
    void SetPixelShader(const FWString& filename, const FString& funcname, const FString& version);
    
    void ChangeViewMode(EViewModeIndex evi) const;
    
    // CreateBuffer
    void CreateConstantBuffer();
    void CreateLightingBuffer();
    void CreateLitUnlitBuffer();
    ID3D11Buffer* CreateVertexBuffer(FVertexSimple* vertices, UINT byteWidth) const;
    ID3D11Buffer* CreateVertexBuffer(const TArray<FVertexSimple>& vertices, UINT byteWidth) const;
    ID3D11Buffer* CreateVertexBuffer(const TArray<FVector>& vertices, UINT byteWidth) const;
    ID3D11Buffer* CreateIndexBuffer(uint32* indices, UINT byteWidth) const;
    ID3D11Buffer* CreateIndexBuffer(const TArray<uint32>& indices, UINT byteWidth) const;

    // Setup
    void BindBuffers();
    
    // update
    void UpdateLightBuffer() const;
    void UpdateConstant(const FMatrix& WorldMatrix, FVector4 UUIDColor, bool IsSelected) const;
    void UpdateConstantDeferred(ID3D11DeviceContext* Context, const FMatrix& WorldMatrix, FVector4 UUIDColor, bool IsSelected) const;

    void UpdateViewMatrix(const FMatrix& ViewMatrix) const;
    void UpdateProjectionMatrix(const FMatrix& ProjectionMatrix) const;
    
    void UpdateMaterial(const FObjMaterialInfo& MaterialInfo) const;
    void UpdateMaterialDeferred(ID3D11DeviceContext* Context, const FObjMaterialInfo& MaterialInfo) const;
    void UpdateLitUnlitConstant(int isLit) const;
    void UpdateIsGizmoConstant(int IsGizmo) const;
    void UpdateSubMeshConstant(bool isSelected) const;
    void UpdateTextureConstant(float UOffset, float VOffset);

public://텍스쳐용 기능 추가
    ID3D11VertexShader* VertexTextureShader = nullptr;
    ID3D11PixelShader* PixelTextureShader = nullptr;
    ID3D11InputLayout* TextureInputLayout = nullptr;

    uint32 TextureStride;
    struct FSubUVConstant
    {
        float indexU;
        float indexV;
    };
    ID3D11Buffer* SubUVConstantBuffer = nullptr;

public:
    void CreateTextureShader();
    void ReleaseTextureShader();
    void PrepareTextureShader() const;
    ID3D11Buffer* CreateVertexTextureBuffer(FVertexTexture* vertices, UINT byteWidth) const;
    ID3D11Buffer* CreateIndexTextureBuffer(uint32* indices, UINT byteWidth) const;
    void RenderTexturePrimitive(ID3D11Buffer* pVertexBuffer, UINT numVertices,
        ID3D11Buffer* pIndexBuffer, UINT numIndices,
        ID3D11ShaderResourceView* _TextureSRV,
        ID3D11SamplerState* _SamplerState) const;
    void RenderTextPrimitive(ID3D11Buffer* pVertexBuffer, UINT numVertices,
        ID3D11ShaderResourceView* _TextureSRV,
        ID3D11SamplerState* _SamplerState) const;
    ID3D11Buffer* CreateVertexBuffer(FVertexTexture* vertices, UINT byteWidth) const;

    void UpdateSubUVConstant(float _indexU, float _indexV) const;
    void PrepareSubUVConstant() const;


public: // line shader
    void PrepareLineShader() const;
    void CreateLineShader();
    void ReleaseLineShader() const;
    void RenderBatch(const FGridParameters& gridParam, ID3D11Buffer* pVertexBuffer, int boundingBoxCount, int coneCount, int coneSegmentCount, int obbCount) const;
    void UpdateGridConstantBuffer(const FGridParameters& gridParams) const;
    void UpdateLinePrimitveCountBuffer(int numBoundingBoxes, int numCones) const;
    ID3D11Buffer* CreateStaticVerticesBuffer() const;
    ID3D11Buffer* CreateBoundingBoxBuffer(UINT numBoundingBoxes) const;
    ID3D11Buffer* CreateOBBBuffer(UINT numBoundingBoxes) const;
    ID3D11Buffer* CreateConeBuffer(UINT numCones) const;
    ID3D11ShaderResourceView* CreateBoundingBoxSRV(ID3D11Buffer* pBoundingBoxBuffer, UINT numBoundingBoxes);
    ID3D11ShaderResourceView* CreateOBBSRV(ID3D11Buffer* pBoundingBoxBuffer, UINT numBoundingBoxes);
    ID3D11ShaderResourceView* CreateConeSRV(ID3D11Buffer* pConeBuffer, UINT numCones);

    void UpdateBoundingBoxBuffer(ID3D11Buffer* pBoundingBoxBuffer, const TArray<FBoundingBox>& BoundingBoxes, int numBoundingBoxes) const;
    void UpdateOBBBuffer(ID3D11Buffer* pBoundingBoxBuffer, const TArray<FOBB>& BoundingBoxes, int numBoundingBoxes) const;
    void UpdateConesBuffer(ID3D11Buffer* pConeBuffer, const TArray<FCone>& Cones, int numCones) const;

    //Render Pass Demo
    void SetViewport(std::shared_ptr<FEditorViewportClient> InActiveViewport);
    void SetWorld(UWorld* InWorld);
    void PrepareRender(bool bShouldUpdateRender);
    bool IsInsideFrustum(UStaticMeshComponent* StaticMeshComp) const;
    void ClearRenderArr();
    void Render();
    void RenderStaticMeshes();
    void RenderGizmos();
    void RenderLight();
    void RenderBillboards();

    // early uuid rendering
private:
    HRESULT CreateUAV();
    void ReleaseUAV();

    void DiscardByUUID(const TArray<UPrimitiveComponent*>& InComponent, TArray<UPrimitiveComponent*>& OutComponent);
    void PrepareRenderUUID(ID3D11DeviceContext* Context);
    void RenderUUID(const TArray<UPrimitiveComponent*>& InComponent, ID3D11DeviceContext* Context);
    std::unordered_set<UINT> ReadValidUUID();
    ID3D11VertexShader* UUIDVertexShader = nullptr;
    ID3D11PixelShader* UUIDPixelShader = nullptr;
    ID3D11InputLayout* UUIDInputLayout = nullptr;
    ID3D11Texture2D* UUIDMapTexture = nullptr; // uuid를 그릴 버퍼
    ID3D11Texture2D* UUIDStagingTexture = nullptr;
    ID3D11RenderTargetView* UUIDTextureRTV = nullptr;

    // world 생성시 batch용
private:
    TArray<TArray<UStaticMeshComponent*>> AggregateMeshComponents(FOctreeNode* Octree, uint32 MaxAggregateNum);

private:
    struct FMeshData // 렌더러 내부에서만 사용하므로 여기에서 선언
    {
        uint32 IndexStart;
        uint32 IndexCount;
        FMatrix WorldMatrix;
        bool bIsSelected;
    };

    /**
     * Key: 머티리얼
     * Value: 해당 머티리얼을 사용하는 서브메시의 배열
     */
    std::unordered_map<UMaterial*, std::unordered_map<UStaticMesh*, std::vector<FMeshData>>> MaterialMeshMap; 
    void SortByMaterial(TArray<UPrimitiveComponent*> PrimComps);
    std::unordered_map<UMaterial*, std::unordered_map<UStaticMesh*, std::vector<FRenderer::FMeshData>>>
        SortByMaterialThread(TArray<UPrimitiveComponent*>& PrimComps,
        uint32 start, uint32 end);

    TArray<UGizmoBaseComponent*> GizmoObjs;
    TArray<UBillboardComponent*> BillboardObjs;
    TArray<ULightComponentBase*> LightObjs;

public:
    ID3D11VertexShader* VertexLineShader = nullptr;
    ID3D11PixelShader* PixelLineShader = nullptr;
    ID3D11Buffer* GridConstantBuffer = nullptr;
    ID3D11Buffer* LinePrimitiveBuffer = nullptr;
    ID3D11ShaderResourceView* pBBSRV = nullptr;
    ID3D11ShaderResourceView* pConeSRV = nullptr;
    ID3D11ShaderResourceView* pOBBSRV = nullptr;

// thread
private:
    void RenderStaticMeshesThread(
        std::vector<FMeshData> DataArray, 
        size_t i, size_t end, size_t tid, 
        UMaterial* Material, const UStaticMesh* StaticMesh,
         ID3D11CommandList* &CommandList);

#pragma region quad
private:
    std::shared_ptr<FEditorViewportClient> ActiveViewport;
    UWorld* World = nullptr;

    struct FQuadVertex
    {
        float X, Y, Z, U, V;
    };

    FQuadVertex QuadVertices[4] = {
        {-1.f, 1.f, 0.f, 0.f, 0.f},
        {1.f, 1.f, 0.f, 1.f, 0.f },
        {-1.f, -1.f, 0.f, 0.f, 1.f},
        {1.f, -1.f, 0.f, 1.f, 1.f}
    };

    uint32 QuadIndides[6] = { 0, 1, 2, 2, 1, 3 };

    ID3D11Buffer* QuadVertexBuffer = nullptr;
    ID3D11Buffer* QuadIndexBuffer = nullptr;

    FLOAT ClearColor[4] = { 0.15f, 0.15f, 0.15f, 1.0f };
    ID3D11Texture2D* QuadTexture = nullptr;
    ID3D11RenderTargetView* QuadRTV = nullptr;
    ID3D11ShaderResourceView* QuadTextureSRV = nullptr;
    ID3D11SamplerState* QuadSampler = nullptr;

    ID3D11VertexShader* QuadVertexShader = nullptr;
    ID3D11PixelShader* QuadPixelShader = nullptr;
    ID3D11InputLayout* QuadInputLayout = nullptr;

    void CreateQuad();
    void ReleaseQuad();

public:
    void PrepareQuad();
    void RenderQuad();

    void PrepareResize();
    void OnResize(const DXGI_SWAP_CHAIN_DESC& SwapchainDesc);
#pragma endregion quad
};

