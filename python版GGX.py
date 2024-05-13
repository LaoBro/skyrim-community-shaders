import math

def FastfresnelSchlick(LoH, F0):
    return F0 + (1 - F0) * math.exp2((-5.55473 * LoH - 6.98316) * LoH)

def Vis_SmithJointApprox_invert(NoV, NoL, a):
    Vis_SmithV = NoL * ( NoV * ( 1 - a ) + a )
    Vis_SmithL = NoV * ( NoL * ( 1 - a ) + a )
    return Vis_SmithV + Vis_SmithL

def bxdf_ggx_D_invert(NoH, a2):
    tmp = (NoH * a2 - NoH) * NoH + 1.0
    return tmp * tmp

def GGX(NoL, NoV, NoH, LoH, Roughness, F0):
   
    a = Roughness * Roughness
    a2 = a * a

    D = bxdf_ggx_D_invert(NoH, a2)
    V = Vis_SmithJointApprox_invert(NoV, NoL, a)
    F = FastfresnelSchlick(LoH, F0)

    numerator = a2 * F * 0.5
    denominator = D * V
    kS = numerator / denominator

    return kS * NoL

LoH1 = NoL1 = NoV1 = 1 /  2
NoH1 = 1
F01 = 0.04
Roughness1 = 0.4

print((Roughness1, GGX(NoL1, NoV1, NoH1, LoH1, Roughness1, F01)))