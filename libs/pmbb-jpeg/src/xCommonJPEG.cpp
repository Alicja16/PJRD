/*
    SPDX-FileCopyrightText: 2020-2024 Jakub Stankowski <jakub.stankowski@put.poznan.pl>
    SPDX-License-Identifier: BSD-3-Clause
*/
#include "xCommonDefJPEG.h"
#include "xString.h"

namespace PMBB_NAMESPACE::JPEG {

//=============================================================================================================================================================================

eImpl xStrToImpl(const std::string& Impl)
{
  std::string ImplL = xString::toLower(Impl);
  return ImplL == "simple"   ? eImpl::Simple   :
         ImplL == "advanded" ? eImpl::Advanced :
                               eImpl::INVALID  ;
}
std::string xImplToStr(eImpl Impl)
{
  return Impl == eImpl::Simple   ? "Simple"   :
         Impl == eImpl::Advanced ? "Advanced" :
                                   "INVALID"  ;
}
eQTLa xStrToQTLa(const std::string& QTLa)
{
  std::string QTLaL = xString::toLower(QTLa);
  return QTLaL == "default"  ? eQTLa::Default  :
         QTLaL == "flat"     ? eQTLa::Flat     :
         QTLaL == "semiflat" ? eQTLa::SemiFlat :
                               eQTLa::INVALID  ;
}
std::string xQTLaToStr(eQTLa QTLa)
{
  return QTLa == eQTLa::Default  ? "Default"  :
         QTLa == eQTLa::Flat     ? "Flat"     :
         QTLa == eQTLa::SemiFlat ? "SemiFlat" :
                                   "INVALID"  ;
}
eCalkMd xStrToCalkMd(const std::string& CalkMd)
{
  std::string CalkMdL = xString::toLower(CalkMd);
  return CalkMdL == "exact"  ? eCalkMd::Exact  :
         CalkMdL == "approx" ? eCalkMd::Approx :
                               eCalkMd::INVALID;
}
std::string xCalkMdToStr(eCalkMd CalkMd)
{
  switch(CalkMd)
  {
  case eCalkMd::Exact : return "Exact" ; 
  case eCalkMd::Approx: return "Approx";
  default: return "INVALID";
  }
}
eLmbd xStrToLmbd(const std::string& Lmbd)
{
    std::string LmbdL = xString::toLower(Lmbd);
  return LmbdL == "exhaustive"  ? eLmbd::Exhaustive  :
         LmbdL == "approxexact" ? eLmbd::ApproxExact :
         LmbdL == "approxfast"  ? eLmbd::ApproxFast  :
                                  eLmbd::INVALID    ;
}
std::string xLmbdToStr(eLmbd Lmbd)
{
  switch(Lmbd)
  {
  case eLmbd::Exhaustive : return "Exhaustive" ; 
  case eLmbd::ApproxExact: return "ApproxExact";
  case eLmbd::ApproxFast : return "ApproxFast" ;
  default: return "INVALID";
  }
}

//=============================================================================================================================================================================

} //end of namespace PMBB::JPEG