// ImGui - standalone example application for DirectX 11
// If you are new to ImGui, see examples/README.txt and documentation at the top of imgui.cpp.

#define _CRT_SECURE_NO_WARNINGS

#include "imgui.h"
#include "imgui_impl_dx11.h"
#include <d3d11.h>
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#include <tchar.h>
#include <math.h>
#include <vector>
#include <string>
#include <stack>
#include <unordered_map>
#include <regex>

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui_internal.h"
#include <memory>
#include <iostream>
#include <algorithm>
#include <tuple>

// Data
static ID3D11Device*            g_pd3dDevice = NULL;
static ID3D11DeviceContext*     g_pd3dDeviceContext = NULL;
static IDXGISwapChain*          g_pSwapChain = NULL;
static ID3D11RenderTargetView*  g_mainRenderTargetView = NULL;

void CreateRenderTarget()
{
  DXGI_SWAP_CHAIN_DESC sd;
  g_pSwapChain->GetDesc(&sd);

  // Create the render target
  ID3D11Texture2D* pBackBuffer;
  D3D11_RENDER_TARGET_VIEW_DESC render_target_view_desc;
  ZeroMemory(&render_target_view_desc, sizeof(render_target_view_desc));
  render_target_view_desc.Format = sd.BufferDesc.Format;
  render_target_view_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
  g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);
  g_pd3dDevice->CreateRenderTargetView(pBackBuffer, &render_target_view_desc, &g_mainRenderTargetView);
  pBackBuffer->Release();
}

void CleanupRenderTarget()
{
  if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = NULL; }
}

HRESULT CreateDeviceD3D(HWND hWnd)
{
  // Setup swap chain
  DXGI_SWAP_CHAIN_DESC sd;
  ZeroMemory(&sd, sizeof(sd));
  sd.BufferCount = 2;
  sd.BufferDesc.Width = 0;
  sd.BufferDesc.Height = 0;
  sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  sd.BufferDesc.RefreshRate.Numerator = 60;
  sd.BufferDesc.RefreshRate.Denominator = 1;
  sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
  sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  sd.OutputWindow = hWnd;
  sd.SampleDesc.Count = 1;
  sd.SampleDesc.Quality = 0;
  sd.Windowed = TRUE;
  sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

  UINT createDeviceFlags = 0;
  //createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
  D3D_FEATURE_LEVEL featureLevel;
  const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
  if (D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext) != S_OK)
    return E_FAIL;

  CreateRenderTarget();

  return S_OK;
}

void CleanupDeviceD3D()
{
  CleanupRenderTarget();
  if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = NULL; }
  if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = NULL; }
  if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = NULL; }
}

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
    return true;

  switch (msg)
  {
  case WM_SIZE:
    if (g_pd3dDevice != NULL && wParam != SIZE_MINIMIZED)
    {
      ImGui_ImplDX11_InvalidateDeviceObjects();
      CleanupRenderTarget();
      g_pSwapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
      CreateRenderTarget();
      ImGui_ImplDX11_CreateDeviceObjects();
    }
    return 0;
  case WM_SYSCOMMAND:
    if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
      return 0;
    break;
  case WM_DESTROY:
    PostQuitMessage(0);
    return 0;
  }
  return DefWindowProc(hWnd, msg, wParam, lParam);
}

static void ShowExampleAppCustomNodeGraph(bool* opened);
void DrawNodeGraph(ImGuiIO& io);

void MakeGraph();
void DeserializeGraph();

int main(int, char**)
{
  // Create application window
  WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(NULL), NULL, LoadCursor(NULL, IDC_ARROW), NULL, NULL, _T("CAN Editor"), NULL };
  RegisterClassEx(&wc);
  HWND hwnd = CreateWindow(_T("CAN Editor"), _T("CAN Editor"), WS_OVERLAPPEDWINDOW, 100, 100, 1280, 800, NULL, NULL, wc.hInstance, NULL);

  // Initialize Direct3D
  if (CreateDeviceD3D(hwnd) < 0)
  {
    CleanupDeviceD3D();
    UnregisterClass(_T("CAN Editor"), wc.hInstance);
    return 1;
  }

  // Show the window
  ShowWindow(hwnd, SW_SHOWDEFAULT);
  UpdateWindow(hwnd);

  // Setup ImGui binding
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO(); (void)io;
  ImGui_ImplDX11_Init(hwnd, g_pd3dDevice, g_pd3dDeviceContext);
  //io.NavFlags |= ImGuiNavFlags_EnableKeyboard;  // Enable Keyboard Controls

  // Setup style
  ImGui::StyleColorsDark();
  ImGuiStyle& style = ImGui::GetStyle();
  style.WindowRounding = 0.0f;

  bool show_demo_window = true;
  bool show_another_window = false;
  ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

  // Main loop
  MSG msg;
  ZeroMemory(&msg, sizeof(msg));
  // MakeGraph();
  DeserializeGraph();
  while (msg.message != WM_QUIT)
  {
    if (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE))
    {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
      continue;
    }
    ImGui_ImplDX11_NewFrame();

    DrawNodeGraph(io);

    // Rendering
    g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, NULL);
    g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, (float*)&clear_color);
    ImGui::Render();

    g_pSwapChain->Present(1, 0); // Present with vsync
                                 //g_pSwapChain->Present(0, 0); // Present without vsync
  }

  ImGui_ImplDX11_Shutdown();
  ImGui::DestroyContext();

  CleanupDeviceD3D();
  UnregisterClass(_T("ImGui Example"), wc.hInstance);

  return 0;
}

class Node;
class SlotType;
class Slot;

class SlotConnection
{
public:
  std::shared_ptr<Slot> mInput;
  std::shared_ptr<Slot> mOutput;

  bool operator==(const SlotConnection& sc) const
  {
    return mInput == sc.mInput && mOutput == sc.mOutput;
  }

  ImVec2 mControlPoint;

  SlotConnection(std::shared_ptr<Slot> input, std::shared_ptr<Slot> output);
};

class Slot : public std::enable_shared_from_this<Slot>
{
  std::vector<std::shared_ptr<SlotConnection>> mConnectedTo;
  std::shared_ptr<SlotType> mType;
  std::string mSlotName;

  std::shared_ptr<Node> mOwner;

  int mNodeIndex;
  bool mIsInput;

  friend class SlotType;
  friend class NodeType;

  void ConnectOther(std::shared_ptr<SlotConnection> sc)
  {
    if(mIsInput)
    {
      mConnectedTo.clear();
    }

    mConnectedTo.push_back(sc);
  }

  void DisconnectOther(std::shared_ptr<SlotConnection> in)
  {
    mConnectedTo.erase(std::remove(mConnectedTo.begin(), mConnectedTo.end(), in), mConnectedTo.end());
  }

public:

  std::shared_ptr<Node> GetOwner() { return mOwner; }

  void Disconnect(std::shared_ptr<SlotConnection> in)
  {
    mConnectedTo.erase(std::remove(mConnectedTo.begin(), mConnectedTo.end(), in), mConnectedTo.end());

    if(!mIsInput)
    {
      in->mInput->DisconnectOther(in);
    }
    else
    {
      in->mOutput->DisconnectOther(in);
    }
  }

  void ClearConnections()
  {
    for(auto connection : mConnectedTo)
    {
      if (!mIsInput)
      {
        connection->mInput->DisconnectOther(connection);
      }
      else
      {
        connection->mOutput->DisconnectOther(connection);
      }
    }

    mConnectedTo.clear();
  }

  std::shared_ptr<SlotConnection> Connect(std::shared_ptr<Slot> in, ImVec2 controlPoint)
  {
    std::shared_ptr<SlotConnection> sc = Connect(in);
    sc->mControlPoint = controlPoint;

    return sc;
  }

  std::shared_ptr<SlotConnection> Connect(std::shared_ptr<Slot> in)
  {
    if(!mIsInput)
    {
      std::shared_ptr<SlotConnection> sc = std::make_shared<SlotConnection>(in, shared_from_this());

      mConnectedTo.push_back(sc);

      in->ConnectOther(sc);
      return sc;
    }
    else
    {
      mConnectedTo.clear();
      std::shared_ptr<SlotConnection> sc = std::make_shared<SlotConnection>(shared_from_this(), in);

      mConnectedTo.push_back(sc);

      in->ConnectOther(sc);
      return sc;
    }
  }

  ImVec2 GetCenter() const;

  bool IsConnected() const { return mConnectedTo.empty(); }
  bool CanConnect(std::shared_ptr<Slot> other);
  std::vector<std::shared_ptr<SlotConnection>> GetConnections() const { return mConnectedTo; }
  std::string GetName() const { return mSlotName; }

  int GetNodeIndex() const { return mNodeIndex; }
  bool IsInput() const { return mIsInput; }

  std::shared_ptr<SlotType> GetSlotType() const { return mType; }
};

SlotConnection::SlotConnection(std::shared_ptr<Slot> input, std::shared_ptr<Slot> output) : mInput(input), mOutput(output)
{
  ImVec2 p1 = input->GetCenter();
  ImVec2 p2 = output->GetCenter();
  mControlPoint = (p1 - p2) / 2 + p2;
}

class SlotType : public std::enable_shared_from_this<SlotType>
{
  std::string mTypeName;

  ImU32 mSlotFillColor;
  ImU32 mSlotEdgeColor;
  ImU32 mSlotRoundedness;
  
  ImU32 mHoveredSlotFillColor;
  ImU32 mHoveredSlotEdgeColor;
  ImU32 mHoveredSlotRoundedness;

  size_t mUID;

  friend class NodeType;

protected:

  void MakeSlotHelper(std::shared_ptr<Slot> s, std::string name)
  {
    s->mType = shared_from_this();
    s->mSlotName = name;
  }

public:

  std::string GetTypeName() const { return mTypeName; }

  ImU32 GetSlotFillColor() const { return mSlotFillColor; }
  ImU32 GetSlotEdgeColor() const { return mSlotEdgeColor; }
  ImU32 GetSlotRoudedness() const { return mSlotRoundedness; }

  ImU32 GetHoveredSlotFillColor() const { return mHoveredSlotFillColor; }
  ImU32 GetHoveredSlotEdgeColor() const { return mHoveredSlotEdgeColor; }
  ImU32 GetHoveredSlotRoudedness() const { return mHoveredSlotRoundedness; }

  bool CanConnect(std::shared_ptr<SlotType> other) const { return mUID == other->mUID; }

  virtual std::shared_ptr<Slot> MakeSlot(std::string name)
  {
    std::shared_ptr<Slot> slot = std::make_shared<Slot>();

    MakeSlotHelper(slot, name);
    
    return slot;
  }

  SlotType(std::string name, ImU32 fillColor, ImU32 edgeColor, ImU32 roundedness, ImU32 hFilledColor, ImU32 hSlotEdgeColor, ImU32 hSlotRoundedness)
    : mTypeName(name), mSlotFillColor(fillColor), mSlotEdgeColor(edgeColor), mSlotRoundedness(roundedness), mHoveredSlotFillColor(hFilledColor),
      mHoveredSlotEdgeColor(hSlotEdgeColor), mHoveredSlotRoundedness(hSlotRoundedness), mUID(std::hash<std::string>{}(mTypeName)) {}
};

bool Slot::CanConnect(std::shared_ptr<Slot> other)
{
  std::shared_ptr<SlotConnection> sc;
  if (!mIsInput)
  {
    sc = std::make_shared<SlotConnection>(other, shared_from_this());
  }
  else
  {
    sc = std::make_shared<SlotConnection>(shared_from_this(), other);
  }

  bool isAlreadyConnected = std::find(mConnectedTo.begin(), mConnectedTo.end(), sc) != mConnectedTo.end();
  return !isAlreadyConnected && GetSlotType()->CanConnect(other->GetSlotType()) && IsInput() != other->IsInput() && mOwner != other->mOwner;
}

class NodeType;

ImVec2 gSlotSize = ImVec2(10, 10);
float gSlotPadding = 5;

using NodeUID = unsigned;

NodeUID NewUID()
{
  return rand();
}

class Node
{
  std::vector<std::shared_ptr<Slot>> mInputs;
  std::vector<std::shared_ptr<Slot>> mOutputs;

  std::shared_ptr<NodeType> mType;

  ImVec2 mPosition;
  ImVec2 mSize;

  NodeUID mUID;

  uint64_t mUserData;

  friend class NodeType;

public:

  uint64_t& GetUserData() { return mUserData; }

  std::pair<ImVec2, ImVec2> GetMinAndMax() const 
  {
    return std::make_pair(mPosition - mSize / 2, mPosition + mSize / 2);
  }

  ImVec2 GetCenterPosition() const { return mPosition; }
  ImVec2 GetSize() const { return mSize; }

  std::vector<std::shared_ptr<Slot>> GetInputSlots() const { return mInputs; }
  std::vector<std::shared_ptr<Slot>> GetOutputSlots() const { return mOutputs; }

  std::shared_ptr<NodeType> GetType() const { return mType; }

  NodeUID GetNodeUID() { return mUID; }

  void ApplyDelta(ImVec2 delta) { mPosition += delta; }
  void SetPosition(ImVec2 pos) { mPosition = pos; }

  void SetSize(ImVec2 size) { mSize = size; }
};

ImVec2 Slot::GetCenter() const
{
  auto [nodeMin, nodeMax] = mOwner->GetMinAndMax();

  float x;
  ImVec2 bodyOffset;
  int count;

  if (IsInput())
  {
    x = nodeMin.x;
    count = mOwner->GetInputSlots().size();
    bodyOffset = nodeMin + mOwner->GetSize() * ImVec2(0, 0.5f);
  }
  else
  {
    x = nodeMax.x;
    count = mOwner->GetOutputSlots().size();
    bodyOffset = nodeMax - mOwner->GetSize() * ImVec2(0, 0.5f);
  }

  float yDelta = gSlotSize.y + gSlotPadding;
  int indx = GetNodeIndex();
  ImVec2 startingMin = ImVec2(0, -((count - 1) / 2.0f * gSlotSize.y + (count - 1) / 2.0f * gSlotPadding)) + bodyOffset;

  return startingMin + ImVec2(0, yDelta * indx);
}

class NodeType : public std::enable_shared_from_this<NodeType>
{
  std::vector<std::string> mInputNames;
  std::vector<std::shared_ptr<SlotType>> mInputs;

  std::vector<std::string> mOutputNames;
  std::vector<std::shared_ptr<SlotType>> mOutputs;

  std::string mTypeName;

  ImU32 mFillColor;
  ImU32 mEdgeColor;
  float mRoundedness;

  ImVec2 mNodeSize;

protected:

  virtual uint64_t DeserializeUserData(std::string str) { return 0; }

public:

  virtual void UserDataToPopulateData(uint64_t& userdata, void**& populatedata) { populatedata = nullptr; }

  std::vector<std::shared_ptr<SlotType>> GetInputSlotTypes() const { return mInputs; }
  std::vector<std::shared_ptr<SlotType>> GetOutputSlotsTypes() const { return mOutputs; }
  std::string GetTypeName() const { return mTypeName; }

  virtual void DrawNode(ImDrawList* draw_list, uint64_t& user_data) {}

  void AddInputSlot(std::shared_ptr<SlotType> in, std::string name)
  {
    mInputs.push_back(in);
    mInputNames.push_back(name);
  }

  void AddOutputSlot(std::shared_ptr<SlotType> in, std::string name)
  {
    mOutputs.push_back(in);
    mOutputNames.push_back(name);
  }

  std::shared_ptr<Node> MakeNode(ImVec2 pos, NodeUID uid = NewUID(), uint64_t userdata = 0)
  {
    std::shared_ptr<Node> node = std::make_shared<Node>();

    node->mType = shared_from_this();
    node->mPosition = pos;
    node->mSize = mNodeSize;
    node->mUID = uid;
    node->mUserData = userdata;

    for(int i = 0; i < mInputs.size(); ++i)
    {
      auto slot = mInputs[i]->MakeSlot(mInputNames[i]);
      slot->mOwner = node;
      slot->mIsInput = true;
      slot->mNodeIndex = i;
      node->mInputs.push_back(slot);
    }
    
    for (int i = 0; i < mOutputs.size(); ++i)
    {
      auto slot = mOutputs[i]->MakeSlot(mOutputNames[i]);
      slot->mOwner = node;
      slot->mIsInput = false;
      slot->mNodeIndex = i;
      node->mOutputs.push_back(slot);
    }

    return node;
  }

  ImU32 GetFillColor() const { return mFillColor; }
  ImU32 GetEdgeColor() const { return mEdgeColor; }
  float GetRoundedness() const { return mRoundedness; }

  virtual std::string SerializeUserData(uint64_t& userdata) { return ""; }
  std::shared_ptr<Node> DeserializeNode(std::string data, ImVec2 pos, NodeUID uid, uint64_t userdata = 0) { return MakeNode(pos, uid, DeserializeUserData(data)); }

  NodeType(std::string typeName, ImU32 fillColor, ImU32 edgeColor, float roundedness, ImVec2 size) : mTypeName(typeName), mFillColor(fillColor), mEdgeColor(edgeColor), mRoundedness(roundedness), mNodeSize(size) {}
};

class IntegerLiteralNodeType : public NodeType
{
protected:

  void UserDataToPopulateData(uint64_t& userdata, void**& populatedata)
  {
    populatedata[0] = static_cast<void*>(&userdata);
  }

  uint64_t DeserializeUserData(std::string str) override
  {
    int data = atoi(str.c_str());

    return data;
  }

  virtual void DrawNode(ImDrawList* draw_list, uint64_t& user_data)
  {
    ImGui::Text("Value: ");
    ImGui::SameLine();
    ImGui::InputInt("", reinterpret_cast<int*>(&user_data));
  }
  
public:

  virtual std::string SerializeUserData(uint64_t& data)
  {
    return std::to_string(data);
  }

  IntegerLiteralNodeType(std::string typeName, ImU32 fillColor, ImU32 edgeColor, float roundedness) : NodeType(typeName, fillColor, edgeColor, roundedness, ImVec2(200, 100)) {}
};

std::vector<std::shared_ptr<Node>> gNodes;

ImU32 gDefaultNodeFillColor = IM_COL32(0, 0, 38, 255);
ImU32 gDefaultNodeEdgeColor = IM_COL32(100, 100, 100, 255);
float gDefaultNodeRoundedness = 2.0f;

ImU32 gDefaultSlotFillColor = IM_COL32(100, 100, 100, 255);
ImU32 gDefaultSlotEdgeColor = IM_COL32(255, 255, 255, 255);
float gDefaultSlotRoundedness = 2.0f;

ImU32 gDefaultHoveredSlotFillColor = IM_COL32(255, 255, 255, 255);
ImU32 gDefaultHoveredSlotEdgeColor = IM_COL32(255, 255, 255, 255);
float gDefaultHoveredSlotRoundedness = 0.0f;

ImU32 gConnectionColor = IM_COL32(255, 255, 0, 255);
ImU32 gConnectionHoveredColor = IM_COL32(100, 100, 0, 255);
float gConnectionWidth = 5.0f;
float gConnectionHoveredDistSq = 50.0f;
float gConnectionControlRadius = 18.0f;
ImU32 gConnectionControlColor = IM_COL32(126, 126, 126, 255);

void DrawLinedRectangle(ImDrawList* draw_list, ImVec2 min, ImVec2 max, ImU32 fillColor, ImU32 edgeColor, float roundedness)
{
  draw_list->AddRectFilled(min, max, fillColor, roundedness);
  draw_list->AddRect(min, max, edgeColor, roundedness);
}

bool gResetHoverHandled = true;
bool gHoverHandled = false; 

std::shared_ptr<Slot> gSlot;
bool gSlotDragging;
std::shared_ptr<Slot> gLastAvailableSlot;
bool gAvailableSlotHovered = false;

void OnSlotDown(std::shared_ptr<Slot> slot)
{
  gSlot = slot;
  gSlotDragging = true;
  gResetHoverHandled = false;
}

void OnSlotUp()
{
  gSlotDragging = false;
  gResetHoverHandled = true;

  if(gAvailableSlotHovered)
  {
    gSlot->Connect(gLastAvailableSlot);
  }
}

std::shared_ptr<Node> gDraggingNode;
bool gNodeDragging;

void OnNodeDown(std::shared_ptr<Node> node)
{
  gDraggingNode = node;
  gNodeDragging = true;
  gResetHoverHandled = false;
}

void OnNodeUp()
{
  gNodeDragging = false;
  gResetHoverHandled = true;
}

bool gConnectionHovered = false;
bool gConnectionDragging = false;
std::shared_ptr<SlotConnection> gHoveredConnection;

void OnConnectionUp()
{
  gConnectionHovered = false;
  gConnectionDragging = false;
}

bool IsDraggingBusy()
{
  return gSlotDragging || gNodeDragging || gConnectionDragging;
}

std::stack<std::shared_ptr<SlotConnection>> gToDraw;

bool gRightClickHandled = false;

void ActualDrawConnections(ImDrawList* draw_list)
{
  while(!gToDraw.empty())
  {
    std::shared_ptr<SlotConnection> sc = gToDraw.top();
    ImVec2 p1 = sc->mInput->GetCenter();
    ImVec2 p2 = sc->mOutput->GetCenter();

    ImGui::SetCursorScreenPos(sc->mControlPoint - ImVec2(gConnectionControlRadius*1.5f, gConnectionControlRadius*1.5f));
    ImGui::InvisibleButton(std::to_string((long long)&sc).c_str(), ImVec2(2.0f * gConnectionControlRadius*1.5f, 2.0f * gConnectionControlRadius*1.5f));
    draw_list->AddCircleFilled(sc->mControlPoint, gConnectionControlRadius, gConnectionControlColor);

    if(!IsDraggingBusy() && !gConnectionHovered && ImGui::IsItemHoveredRect())
    {
      gConnectionHovered = true;
      gHoveredConnection = sc;

      if(ImGui::IsMouseClicked(0))
      {
        gConnectionDragging = true;
      }

      // Reset control points
      if(ImGui::IsMouseClicked(1) && !gRightClickHandled)
      {
        gRightClickHandled = true;

        ImVec2 p1 = sc->mInput->GetCenter();
        ImVec2 p2 = sc->mOutput->GetCenter();
        sc->mControlPoint = (p1 - p2) / 2 + p2;
      }

      if(ImGui::IsMouseDoubleClicked(0))
      {
        sc->mInput->Disconnect(sc);
      }
      else
      {
        draw_list->AddBezierCurve(p1, sc->mControlPoint, sc->mControlPoint, p2, gConnectionHoveredColor, gConnectionWidth);
        gToDraw.pop();
        continue;
      }
    }
    else if(gConnectionDragging)
    {
      if(gHoveredConnection == sc)
      {
        draw_list->AddBezierCurve(p1, sc->mControlPoint, sc->mControlPoint, p2, gConnectionHoveredColor, gConnectionWidth);
        gToDraw.pop();
        continue;
      }
    }

    draw_list->AddBezierCurve(p1, sc->mControlPoint, sc->mControlPoint, p2, gConnectionColor, gConnectionWidth);
    gToDraw.pop();
  }
}

void DrawSlotConnection(ImDrawList* draw_list, std::shared_ptr<SlotConnection> sc)
{
  gToDraw.push(sc);
}

void MakeSlot(ImDrawList* draw_list, const std::shared_ptr<Slot>& slot)
{
  std::shared_ptr<SlotType> type = slot->GetSlotType();

  ImVec2 center = slot->GetCenter();
  ImVec2 min = center - gSlotSize / 2;
  ImVec2 max = center + gSlotSize / 2;

  // Arbitraty, just chosing one so that we don't draw two
  if (!slot->IsInput())
  {
    auto connections = slot->GetConnections();

    for (std::shared_ptr<SlotConnection>& sc : connections)
    {
      DrawSlotConnection(draw_list, sc);
    }
  }

  ImGui::SetCursorScreenPos(min);
  ImGui::InvisibleButton(std::to_string((long long)slot.get()).c_str(), gSlotSize);

  bool highlight = false;

  if (ImGui::IsItemHoveredRect())
  {
    if (!gHoverHandled)
    {
      gHoverHandled = true;
      highlight = true;

      if (ImGui::IsMouseClicked(0))
      {
        OnSlotDown(slot);
      }
    }
    else
    {
      if(gSlotDragging && gSlot->CanConnect(slot))
      {
        highlight = true;

        gAvailableSlotHovered = true;
        gLastAvailableSlot = slot;
      }
    }
  }

  if (highlight)
  {
    DrawLinedRectangle(draw_list, min, max, type->GetHoveredSlotFillColor(), type->GetHoveredSlotEdgeColor(), type->GetHoveredSlotRoudedness());
  }
  else
  {
    DrawLinedRectangle(draw_list, min, max, type->GetSlotFillColor(), type->GetSlotEdgeColor(), type->GetSlotRoudedness());
  }

  draw_list->AddText(max - ImVec2(0, gSlotSize.y * 1.25f), IM_COL32(255, 255, 255, 255), std::string(slot->GetName() + " : " + type->GetTypeName()).c_str());
}

std::shared_ptr<Node> gNodeContextMenuNode;
bool gOpenNodeContextMenu = false;

void MakeNode(ImDrawList* draw_list, std::shared_ptr<Node> node)
{
  auto inputs = node->GetInputSlots();
  auto outputs = node->GetOutputSlots();
  int inputCount = inputs.size();
  int outputCount = outputs.size();

  float inputBreadth = inputCount * gSlotSize.y + (inputCount - 1) * gSlotPadding;
  float outputBreadth = outputCount * gSlotSize.y + (outputCount - 1) * gSlotPadding;

  ImVec2 preferredSize = node->GetSize();
  ImVec2 centeredAt = node->GetCenterPosition();

  preferredSize.y = max(max(inputBreadth, outputBreadth), preferredSize.y);

  // Node Body
  ImVec2 bodyMin = centeredAt - preferredSize / 2;
  ImVec2 bodyMax = centeredAt + preferredSize / 2;

  // Body
  std::shared_ptr<NodeType> nodeType = node->GetType();
  
  DrawLinedRectangle(draw_list, bodyMin, bodyMax, nodeType->GetFillColor(), nodeType->GetEdgeColor(), nodeType->GetRoundedness());

  // Node Input
  for (auto input : inputs)
  {
    MakeSlot(draw_list, input);
  }

  // Node Output
  for (auto output : outputs)
  {
    MakeSlot(draw_list, output);
  }

  draw_list->AddText(bodyMin + ImVec2(5, 5), IM_COL32(255, 255, 255, 255), node->GetType()->GetTypeName().c_str());

  ImGui::SetCursorScreenPos(bodyMin + ImVec2(5, 25));
  ImGui::BeginChild(std::to_string((long long)node.get()).c_str(), preferredSize, false, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);
  nodeType->DrawNode(draw_list, node->GetUserData());
  ImGui::EndChild();

  // Body Logic
  ImGui::SetCursorScreenPos(bodyMin);
  ImGui::InvisibleButton(std::to_string((long long)node.get()).c_str(), preferredSize);
  if ((!gHoverHandled || gNodeDragging) && ImGui::IsItemHoveredRect() && ImGui::IsMouseClicked(0))
  {
    gHoverHandled = true;

    OnNodeDown(node);
  }

  if(ImGui::IsItemHoveredRect() && !gRightClickHandled && ImGui::IsMouseClicked(1))
  {
    gRightClickHandled = true;
    gOpenNodeContextMenu = true;
    gNodeContextMenuNode = node;
  }
}

std::string Serialize();

bool gOpenAddNodeDialog = false;
bool gSerializeDebugPopup = false;
std::string gSerializeDebugPopupString;

bool gToCPPDebugPopup = false;
std::string gToCPPString;

std::unordered_map<std::string, std::shared_ptr<NodeType>> gNodeTypes;

std::string ToCPP();

void DrawNodeGraph(ImGuiIO& io)
{
  ImGui::SetNextWindowSize(io.DisplaySize);
  ImGui::SetNextWindowPos(ImVec2(0, 0));
  ImGui::SetNextWindowCollapsed(false);
  ImGui::Begin("Node Graph", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoSavedSettings |
                                      ImGuiWindowFlags_NoBringToFrontOnFocus);
  {
    // Window State
    static bool show_grid = true;
    static ImVec2 scrolling;

    // Create our child canvas
    ImGui::Text("Hold middle mouse button to scroll (%.2f,%.2f)", scrolling.x, scrolling.y);
    ImGui::SameLine();

    if(ImGui::Button("Serialize"))
    {
      gSerializeDebugPopup = true;
      gSerializeDebugPopupString = Serialize();
    }

    if(ImGui::Button("To CPP"))
    {
      gToCPPDebugPopup = true;
      gToCPPString = ToCPP();
    }

    ImGui::SameLine(ImGui::GetWindowWidth() - 100);
    ImGui::Checkbox("Show grid", &show_grid);
    ImGui::BeginChild("scrolling_region", ImVec2(0, 0), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMove);

    ImVec2 offset = ImGui::GetCursorScreenPos();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    draw_list->ChannelsSplit(2); // bg and fg

    // Draw Grid
    {
      if (show_grid)
      {
        ImU32 GRID_COLOR = IM_COL32(200, 200, 200, 40);
        float GRID_SZ = 64.0f;
        ImVec2 win_pos = ImGui::GetCursorScreenPos();
        ImVec2 canvas_sz = ImGui::GetWindowSize();
        for (float x = fmodf(offset.x, GRID_SZ); x < canvas_sz.x; x += GRID_SZ)
          draw_list->AddLine(ImVec2(x, 0.0f) + win_pos, ImVec2(x, canvas_sz.y) + win_pos, GRID_COLOR);
        for (float y = fmodf(offset.y, GRID_SZ); y < canvas_sz.y; y += GRID_SZ)
          draw_list->AddLine(ImVec2(0.0f, y) + win_pos, ImVec2(canvas_sz.x, y) + win_pos, GRID_COLOR);
      }
    }

    // Draw Nodes
    {
      ImGui::PushID(0);

      // bg
      draw_list->ChannelsSetCurrent(0);
      ActualDrawConnections(draw_list);

      for(auto node : gNodes)
      {
        MakeNode(draw_list, node);
      }
      
      if(gSlotDragging)
      {
        ImVec2 mPos = io.MousePos;
        ImVec2 c = gSlot->GetCenter();
        draw_list->AddLine(mPos, c, gConnectionColor, gConnectionWidth);

        if (!ImGui::IsMouseDown(0))
        {
          OnSlotUp();
        }
      }

      if(gConnectionDragging)
      {
        gHoveredConnection->mControlPoint += io.MouseDelta;

        if (!ImGui::IsMouseDown(0))
        {
          OnConnectionUp();
        }
      }

      if(gNodeDragging)
      {
        ImVec2 mDelta = io.MouseDelta;
        gDraggingNode->ApplyDelta(mDelta);

        if(!ImGui::IsMouseDown(0))
        {
          OnNodeUp();
        }
      }

      ImGui::PopID();
    }

    if(ImGui::IsMouseClicked(1) && !gRightClickHandled)
    {
      ImGui::OpenPopup("gridContextMenu");
    }

    // Context Menus
    if(ImGui::BeginPopup("gridContextMenu"))
    {
      if(ImGui::MenuItem("Add Node"))
      {
        gOpenAddNodeDialog = true;
      }

      ImGui::EndPopup();
    }

    if(gOpenNodeContextMenu)
    {
      ImGui::OpenPopup("nodeContextMenu");
      gOpenNodeContextMenu = false;
    }

    if(ImGui::BeginPopup("nodeContextMenu"))
    {
      if(ImGui::MenuItem("Delete"))
      {
        for(auto input : gNodeContextMenuNode->GetInputSlots())
        {
          input->ClearConnections();
        }
        for(auto output : gNodeContextMenuNode->GetOutputSlots())
        {
          output->ClearConnections();
        }

        gNodes.erase(std::remove(gNodes.begin(), gNodes.end(), gNodeContextMenuNode));
      }

      ImGui::EndPopup();
    }

    // Pop Ups
    // Add Node Dialog
    if(gOpenAddNodeDialog)
    {
      ImGui::Begin("Add Node Dialog", nullptr, ImGuiWindowFlags_NoSavedSettings);

      ImGui::Text("Search: ");
      ImGui::SameLine();

      char searchText[256] = { 0 };
      ImGui::InputText("", searchText, 256);
      std::regex self_regex(searchText, std::regex_constants::ECMAScript | std::regex_constants::icase);

      ImGui::Spacing();

      for(auto& pair : gNodeTypes)
      {
        bool clicked = false;

        // If there's something in the dialog box
        if(searchText[0] != 0)
        {
          if(std::regex_search(pair.first, self_regex))
          {
            clicked = ImGui::Button(pair.first.c_str());
          }
        }
        else
        {
          clicked = ImGui::Button(pair.first.c_str());
        }

        if(clicked)
        {
          // Make node from add node menu!!
          gOpenAddNodeDialog = false;

          gNodes.push_back(pair.second->MakeNode(io.MousePos));
        }
      }

      if(ImGui::Button("Cancel Add Node"))
      {
        gOpenAddNodeDialog = false;
      }

      ImGui::End();
    }

    // Serialize Debug Popup
    if(gSerializeDebugPopup)
    {
      ImGui::Begin("Serialize Debug Popup");

      ImGui::Text(gSerializeDebugPopupString.c_str());

      if(ImGui::Button("Copy"))
      {
        ImGui::SetClipboardText(gSerializeDebugPopupString.c_str());
      }

      ImGui::SameLine();

      if(ImGui::Button("Close"))
      {
        gSerializeDebugPopup = false;
      }

      ImGui::End();
    }

    // To CPP Debug Popup
    if (gToCPPDebugPopup)
    {
      ImGui::Begin("To CPP Debug Popup");

      ImGui::Text("%s", gToCPPString.c_str());

      if (ImGui::Button("Copy"))
      {
        ImGui::SetClipboardText(gToCPPString.c_str());
      }

      ImGui::SameLine();

      if (ImGui::Button("Close"))
      {
        gToCPPDebugPopup = false;
      }

      ImGui::End();
    }

    ImGui::EndChild();
  }
  ImGui::End();

  if(gResetHoverHandled)
    gHoverHandled = false;

  gAvailableSlotHovered = false;
  gConnectionHovered = false;
  gRightClickHandled = false;
}

std::string SerializeNode(std::shared_ptr<Node> node)
{
  std::string data;

  // Serialize Node info
  ImVec2 pos = node->GetCenterPosition();
  std::shared_ptr<NodeType> type = node->GetType();
  data = type->GetTypeName() + ":" + std::to_string(node->GetNodeUID()) + ":" + std::to_string(pos.x) + "," + std::to_string(pos.y) + ":";
  data += type->SerializeUserData(node->GetUserData());

  data += ":";
  // Serialize Slot Connections
  std::vector<std::shared_ptr<Slot>> slots = node->GetInputSlots();
  for(auto slot : slots)
  {
    auto connections = slot->GetConnections();

    if (connections.size() > 0) // prevents popping back the : on accident
    {
      for (std::shared_ptr<SlotConnection>& sc : connections)
      {
        // Node id and slot name
        data += std::to_string(slot->GetNodeIndex()) + "," + std::to_string(sc->mOutput->GetOwner()->GetNodeUID()) + "," + std::to_string(sc->mOutput->GetNodeIndex()) + "," + std::to_string(sc->mControlPoint.x) + "," + std::to_string(sc->mControlPoint.y) + ";";
      }

      data.pop_back(); // remove last ;
    }
  }

  return data;
}

std::string Serialize()
{
  std::string acc;

  for(auto pnode : gNodes)
  {
    acc += SerializeNode(pnode);
    acc += "\n";
  }

  return acc;
}

std::vector<std::string> split(const char *str, char c = ' ')
{
  std::vector<std::string> result;

  do
  {
    const char *begin = str;

    while (*str != c && *str)
      str++;

    result.emplace_back(begin, str);
  } while (0 != *str++);

  return result;
}

std::unordered_map<NodeUID, std::shared_ptr<Node>> gUIDToNode;

struct SlotConnectionSerializationInfo
{
  NodeUID mInputNode;
  unsigned mInputIndex;

  NodeUID mOutputNode;
  unsigned mOutputIndex;

  ImVec2 mControlPoint;
};

std::stack<SlotConnectionSerializationInfo> gConnectionsToMake;

void DeserializeNode(std::string data)
{
  std::vector<std::string> tokens = split(data.c_str(), ':');
  std::string typeName = tokens[0];
  std::string nodeUID = tokens[1];
  std::string position = tokens[2];
  std::string nodeData = tokens[3];
  std::string slotConnectionData = tokens[4];

  auto iType = gNodeTypes.find(typeName);
  if (iType == gNodeTypes.end())
  {
    // Couldn't find type in registered types
    return;
  }

  std::shared_ptr<NodeType> type = iType->second;

  // Make the Node
  ImVec2 vecPos;
  std::vector<std::string> posTokens = split(position.c_str(), ',');
  vecPos.x = std::stof(posTokens[0]);
  vecPos.y = std::stof(posTokens[1]);

  NodeUID uid = std::stoi(nodeUID);
  std::shared_ptr<Node> node = type->DeserializeNode(nodeData, vecPos, uid);
  gNodes.push_back(node);
  gUIDToNode[uid] = node;

  if (!slotConnectionData.empty())
  {
    // Queue up slot connections to make
    std::vector<std::string> slotSetData = split(slotConnectionData.c_str(), ';');
    for (std::string slotData : slotSetData)
    {
      std::vector<std::string> slotTokens = split(slotData.c_str(), ',');

      unsigned inputSlotIndex = std::stoi(slotTokens[0]);
      NodeUID outputNodeUID = std::stoi(slotTokens[1]);
      unsigned outputSlotIndex = std::stoi(slotTokens[2]);

      ImVec2 controlPoint;
      controlPoint.x = std::stof(slotTokens[3]);
      controlPoint.y = std::stof(slotTokens[4]);

      SlotConnectionSerializationInfo info{ uid, inputSlotIndex, outputNodeUID, outputSlotIndex, controlPoint };
      gConnectionsToMake.push(info);
    }
  }
}

void Deserialize(std::string data)
{
  std::vector<std::string> nodeData = split(data.c_str(), '\n');

  // Make Nodes
  for(std::string nd : nodeData)
  {
    if (nd.empty()) continue;
    DeserializeNode(nd);
  }

  // Make Slot Connections
  while(!gConnectionsToMake.empty())
  {
    SlotConnectionSerializationInfo& info = gConnectionsToMake.top();

    std::shared_ptr<Node> inputNode = gUIDToNode[info.mInputNode];
    std::shared_ptr<Node> outputNode = gUIDToNode[info.mOutputNode];

    std::shared_ptr<Slot> inputSlot = inputNode->GetInputSlots()[info.mInputIndex];
    std::shared_ptr<Slot> outputSlot = outputNode->GetOutputSlots()[info.mOutputIndex];

    inputSlot->Connect(outputSlot, info.mControlPoint);

    gConnectionsToMake.pop();
  }
}

void MakeGraph()
{
  std::shared_ptr<SlotType> Integer = std::make_shared<SlotType>("Integer", gDefaultSlotFillColor, gDefaultSlotEdgeColor, gDefaultSlotRoundedness, gDefaultHoveredSlotFillColor, gDefaultHoveredSlotEdgeColor, gDefaultHoveredSlotRoundedness);

  std::shared_ptr<NodeType> Add = std::make_shared<NodeType>("Add", gDefaultNodeFillColor, gDefaultNodeEdgeColor, gDefaultNodeRoundedness, ImVec2(100, 50));

  Add->AddInputSlot(Integer, "a");
  Add->AddInputSlot(Integer, "b");
  Add->AddOutputSlot(Integer, "out");

  std::shared_ptr<IntegerLiteralNodeType> IntegerLiteral = std::make_shared<IntegerLiteralNodeType>("IntegerLiteral", gDefaultNodeFillColor, gDefaultNodeEdgeColor, gDefaultNodeRoundedness);

  IntegerLiteral->AddOutputSlot(Integer, "out");

  std::shared_ptr<NodeType> PrintInteger = std::make_shared<NodeType>("PrintInteger", gDefaultNodeFillColor, gDefaultNodeEdgeColor, gDefaultNodeRoundedness, ImVec2(100, 50));

  PrintInteger->AddInputSlot(Integer, "in");

  // Serialize these in later
  gNodes.push_back(Add->MakeNode(ImVec2(100, 100)));
  gNodes.push_back(IntegerLiteral->MakeNode(ImVec2(100, 100)));
}

void RegisterNodeTypes();

void DeserializeGraph()
{
  RegisterNodeTypes();
  Deserialize("Add:41:805.000000,298.000000::0,18467,0,621.500000,217.750000\n"
              "IntegerLiteral:18467:394.000000,289.000000:6:"

  );
}

#include "CAN.h"

class DumbAllocator final : public CAN::Allocator
{
public:
  DumbAllocator(int size) : mSize(size) {}

  CAN::AllocatorFactory::RelativeBlockHandle Allocate() override
  {
    char* d = new char[mSize];
    return reinterpret_cast<uint64_t>(d);
  }

  void Free(CAN::AllocatorFactory::RelativeBlockHandle handle) override { delete[] reinterpret_cast<char*>(handle); }

  void* GetWeakRef(CAN::AllocatorFactory::RelativeBlockHandle handle) override
  {
    char* d = reinterpret_cast<char*>(handle);
    return d;
  }

  CAN::AllocatorFactory::AllocatorHandle GetAllocatorHandle() override { return reinterpret_cast<uint64_t>(this); }

private:
  int mSize;
};

class DumbAllocatorFactory : public CAN::AllocatorFactory
{
public:
  AllocatorHandle MakeAllocator(size_t size, CAN::NodeTypeGUID uid) override { return reinterpret_cast<uint64_t>(new DumbAllocator(size)); }
  void FreeAllocator(CAN::Allocator* alloc) override { return delete reinterpret_cast<DumbAllocator*>(alloc); }

  CAN::Allocator* LookupAllocator(AllocatorHandle handle) { return reinterpret_cast<CAN::Allocator*>(handle); }
};

#include <functional>

std::unordered_map<std::shared_ptr<NodeType>, CAN::NodeTypeGUID> gNodeTypeLookup;
CAN::RuntimeManager gCANRuntime;
DumbAllocatorFactory gCANAllocatorFactory;

void RegisterNodeTypes()
{
  gCANRuntime.RegisterAllocatorFactory(&gCANAllocatorFactory);
  gCANRuntime.RegisterStandardLibrary();

  auto nodeGUIDS = gCANRuntime.GetNodeGUIDs();

  std::unordered_map<CAN::SlotTypeGUID, std::shared_ptr<SlotType>> typeLookup;

  std::shared_ptr<SlotType> Integer = std::make_shared<SlotType>("Integer", gDefaultSlotFillColor, gDefaultSlotEdgeColor, gDefaultSlotRoundedness, gDefaultHoveredSlotFillColor, gDefaultHoveredSlotEdgeColor, gDefaultHoveredSlotRoundedness);
  typeLookup[CAN::IntegerSlot::TypeGUID] = Integer;

  for(auto& nodeGUID : nodeGUIDS)
  {
    std::string name = gCANRuntime.GetNodeTypeName(nodeGUID);
    std::shared_ptr<NodeType> newNodeType;

    switch(nodeGUID)
    {
    case CAN::IntegerLiteralNode::TypeGUID:
      {
        newNodeType = std::make_shared<IntegerLiteralNodeType>(name, gDefaultNodeFillColor, gDefaultNodeEdgeColor, gDefaultNodeRoundedness);
      }
      break;
    default:
      {
        newNodeType = std::make_shared<NodeType>(name, gDefaultNodeFillColor, gDefaultNodeEdgeColor, gDefaultNodeRoundedness, ImVec2(100, 100));
      }
    }

    auto inputTypes = gCANRuntime.GetInputTypes(nodeGUID);
    auto inputNames = gCANRuntime.GetInputNames(nodeGUID);

    for(int i = 0; i < inputNames.size(); ++i)
    {
      newNodeType->AddInputSlot(typeLookup[inputTypes[i]], inputNames[i]);
    }

    auto outputTypes = gCANRuntime.GetOutputTypes(nodeGUID);
    auto outputNames = gCANRuntime.GetOutputNames(nodeGUID);

    for (int i = 0; i < outputNames.size(); ++i)
    {
      newNodeType->AddOutputSlot(typeLookup[outputTypes[i]], outputNames[i]);
    }

    gNodeTypes[name] = newNodeType;
    gNodeTypeLookup[newNodeType] = nodeGUID;
  }

  /*
  std::shared_ptr<NodeType> Add = std::make_shared<NodeType>("Add", gDefaultNodeFillColor, gDefaultNodeEdgeColor, gDefaultNodeRoundedness, ImVec2(100, 100));

  Add->AddInputSlot(Integer, "a");
  Add->AddInputSlot(Integer, "b");
  Add->AddOutputSlot(Integer, "out");

  std::shared_ptr<IntegerLiteralNodeType> IntegerLiteral = std::make_shared<IntegerLiteralNodeType>("IntegerLiteral", gDefaultNodeFillColor, gDefaultNodeEdgeColor, gDefaultNodeRoundedness);

  IntegerLiteral->AddOutputSlot(Integer, "out");

  std::shared_ptr<NodeType> PrintInteger = std::make_shared<NodeType>("PrintInteger", gDefaultNodeFillColor, gDefaultNodeEdgeColor, gDefaultNodeRoundedness, ImVec2(100, 50));

  PrintInteger->AddInputSlot(Integer, "in");

  gNodeTypes["Add"] = Add;
  gNodeTypes["IntegerLiteral"] = IntegerLiteral;
  gNodeTypes["PrintInteger"] = PrintInteger;*/
}

std::string ToCPP()
{
  std::stack<std::pair<std::shared_ptr<Slot>, int>> toConnect;
  std::unordered_map<std::shared_ptr<Node>, CAN::AllocatorFactory::AbsoluteBlockHandle> nodeLookup;
  std::vector<CAN::AllocatorFactory::AbsoluteBlockHandle> nodeCluster;

  for(auto& n : gNodes)
  {
    // Allocate Node
    void* populatedata[10];
    void** populatedata_ptr = populatedata;

    n->GetType()->UserDataToPopulateData(n->GetUserData(), populatedata_ptr);
    auto block = gCANRuntime.AllocateNode(gNodeTypeLookup[n->GetType()], populatedata_ptr);

    // Register Node Lookup
    nodeLookup[n] = block;
    nodeCluster.push_back(block);

    for(int i = 0; i < n->GetInputSlots().size(); ++i)
    {
      // Register To Connect
      toConnect.push({ n->GetInputSlots()[i], i });
    }
  }

  // Connect All Slots
  while(!toConnect.empty())
  {
    auto[slot, inputIndex] = toConnect.top();

    auto editorOutput = slot->GetConnections()[0]->mOutput->GetOwner();
    auto editorInput = slot->GetConnections()[0]->mInput->GetOwner();

    auto canOutput = nodeLookup[editorOutput];
    auto canInput = nodeLookup[editorInput];

    CAN::Node* rawCanOutput = static_cast<CAN::Node*>(gCANRuntime.GetAllocatorFactory()->GetWeakRef(canOutput));
    CAN::Node* rawCanInput = static_cast<CAN::Node*>(gCANRuntime.GetAllocatorFactory()->GetWeakRef(canInput));

    rawCanInput->GetInputs()[inputIndex]->Connect(rawCanOutput->GetOutputs()[slot->GetConnections()[0]->mOutput->GetNodeIndex()]);

    toConnect.pop();
  }

  CAN::NodeForest forest = CAN::Forestify(CAN::NodeClusterToNodeGraph(&gCANRuntime, nodeCluster));
  return forest.ToCPP();
}

///////////////////////////////////////////////////////////////////////////////
// Todo:
//   Fix Node Hetero/Homogeneous nonsense
//   Organize globals
//   Grid movement
//   Integrate with CAN
//
//-----------------------------------------------------------------------------
//
//   Child window click handling
//   Better add node dialog (default size and position)
//   Multiselect
//   Undo
//   Hide slot names / types
//   Move base types and specific type definitions to different files
//
///////////////////////////////////////////////////////////////////////////////
// Design Notes:
//
// Right now this homogeneous / heterogeneous battle is raging.
// Slots are fully homogeneous, good.
// Nodes are a combination of homogeneous and heterogeneous.
// NodeTypes (which were made to contain all necissary heterogeneous aspects of
// nodes) are heterogeneous.
//
// I should either:
//   Make Nodes Heterogeneous
//   Or
//   Move the rest of the heterogeneous aspects of Nodes to the NodeTypes
//   which would involve using a void* for userData.  Is this a bad thing really?
//
// I actually am liking the second option more right now.  We can get away with
// purely heterogeneous nodes in CAN because the nodes don't contain drawing
// information, in the Editor the actual nodes contain a bunch of information
// that I don't want users to mess with.  So in order to encapsulate properly
// I should move all the non-base node drawing code into the Node Type
// including the sub-window drawing and just pass around a void pointer between
// the homogeneous nodes and the heterogeneous NodeTypes (who will know exactly
// what's in that user data).
//
///////////////////////////////////////////////////////////////////////////////
