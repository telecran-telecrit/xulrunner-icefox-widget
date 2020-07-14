/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Mozilla platform.
 *
 * The Initial Developer of the Original Code is
 * Benjamin Smedberg <benjamin@smedbergs.us>.
 *
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Mozilla Foundation <http://www.mozilla.org/>. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsLayoutStatics.h"
#include "nscore.h"

#include "nsAttrValue.h"
#include "nsAutoCopyListener.h"
#include "nsColorNames.h"
#include "nsComputedDOMStyle.h"
#include "nsContentDLF.h"
#include "nsContentUtils.h"
#include "nsCSSAnonBoxes.h"
#include "nsCSSFrameConstructor.h"
#include "nsCSSKeywords.h"
#include "nsCSSLoader.h"
#include "nsCSSProps.h"
#include "nsCSSPseudoClasses.h"
#include "nsCSSPseudoElements.h"
#include "nsCSSScanner.h"
#include "nsICSSStyleSheet.h"
#include "nsDOMAttribute.h"
#include "nsDOMClassInfo.h"
#include "nsEventListenerManager.h"
#include "nsFrame.h"
#include "nsGenericElement.h"  // for nsDOMEventRTTearoff
#include "nsGenericHTMLElement.h"
#include "nsGlobalWindow.h"
#include "nsHTMLAtoms.h"
#include "nsImageFrame.h"
#include "nsJSEnvironment.h"
#include "nsLayoutAtoms.h"
#include "nsLayoutStylesheetCache.h"
#include "nsNodeInfo.h"
#include "nsRange.h"
#include "nsRepeatService.h"
#include "nsSpaceManager.h"
#include "nsSprocketLayout.h"
#include "nsStackLayout.h"
#include "nsStyleSet.h"
#include "nsTextControlFrame.h"
#include "nsTextTransformer.h"
#include "nsXBLAtoms.h"
#include "nsXBLWindowKeyHandler.h"
#include "nsDOMStorage.h"

#ifdef MOZ_XUL
#include "nsXULContentUtils.h"
#include "nsXULElement.h"
#endif

#ifdef MOZ_MATHML
#include "nsMathMLAtoms.h"
#include "nsMathMLOperators.h"
#endif

#ifdef MOZ_SVG
#include "nsSVGAtoms.h"
#include "nsSVGUtils.h"
#endif

#ifndef MOZ_NO_INSPECTOR_APIS
#include "inDOMView.h"
#endif

#include "nsError.h"
#include "nsTraceRefcnt.h"

static nsrefcnt sLayoutStaticRefcnt;

nsresult
nsLayoutStatics::Initialize()
{
  NS_ASSERTION(sLayoutStaticRefcnt == 0,
               "nsLayoutStatics isn't zero!");

  sLayoutStaticRefcnt = 1;
  NS_LOG_ADDREF(&sLayoutStaticRefcnt, sLayoutStaticRefcnt,
                "nsLayoutStatics", 1);

  nsresult rv;

  nsJSEnvironment::Startup();
  rv = nsContentUtils::Init();
  if (NS_FAILED(rv)) {
    NS_ERROR("Could not initialize nsContentUtils");

    Shutdown();

    return rv;
  }
  rv = nsAttrValue::Init();
  if (NS_FAILED(rv)) {
    NS_ERROR("Could not initialize nsAttrValue");

    Shutdown();

    return rv;
  }

  // Register all of our atoms once
  nsCSSAnonBoxes::AddRefAtoms();
  nsCSSPseudoClasses::AddRefAtoms();
  nsCSSPseudoElements::AddRefAtoms();
  nsCSSKeywords::AddRefTable();
  nsCSSProps::AddRefTable();
  nsColorNames::AddRefTable();
  nsHTMLAtoms::AddRefAtoms();
  nsXBLAtoms::AddRefAtoms();
  nsLayoutAtoms::AddRefAtoms();
  nsXULAtoms::AddRefAtoms();

#ifndef MOZ_NO_INSPECTOR_APIS
  inDOMView::InitAtoms();
#endif

#ifdef MOZ_XUL
  rv = nsXULContentUtils::Init();
  if (NS_FAILED(rv)) {
    NS_ERROR("Could not initialize nsXULContentUtils");

    Shutdown();

    return rv;
  }
#endif

#ifdef MOZ_MATHML
  nsMathMLOperators::AddRefTable();
  nsMathMLAtoms::AddRefAtoms();
#endif

#ifdef MOZ_SVG
  if (nsSVGUtils::SVGEnabled())
    nsContentDLF::RegisterSVG();
  nsSVGAtoms::AddRefAtoms();
#ifdef MOZ_SVG_RENDERER_LIBART
  NS_InitSVGRendererLibartGlobals();
#endif
#ifdef MOZ_SVG_RENDERER_GDIPLUS
  NS_InitSVGRendererGDIPlusGlobals();
#endif
#endif

#ifdef DEBUG
  nsFrame::DisplayReflowStartup();
#endif
  rv = nsTextTransformer::Initialize();
  NS_ENSURE_SUCCESS(rv, rv);

  nsDOMAttribute::Initialize();

  rv = nsDOMStorageManager::Initialize();
  if (NS_FAILED(rv)) {
    NS_ERROR("Could not initialize nsDOMStorageManager");
    return rv;
  }

  return NS_OK;
}

void
nsLayoutStatics::Shutdown()
{
  nsDOMStorageManager::Shutdown();
  nsDOMAttribute::Shutdown();
  nsGenericElement::Shutdown();
  nsEventListenerManager::Shutdown();
  nsContentList::Shutdown();
  nsComputedDOMStyle::Shutdown();
  CSSLoaderImpl::Shutdown();
#ifdef DEBUG
  nsFrame::DisplayReflowShutdown();
#endif

  // Release all of our atoms
  nsColorNames::ReleaseTable();
  nsCSSProps::ReleaseTable();
  nsCSSKeywords::ReleaseTable();
  nsRepeatService::Shutdown();
  nsStackLayout::Shutdown();
  nsBox::Shutdown();

#ifdef MOZ_XUL
  nsXULContentUtils::Finish();
  nsXULElement::ReleaseGlobals();
  nsXULPrototypeElement::ReleaseGlobals();
  nsXULPrototypeScript::ReleaseGlobals();
  nsSprocketLayout::Shutdown();
#endif

#ifdef MOZ_MATHML
  nsMathMLOperators::ReleaseTable();
#endif

#ifdef MOZ_SVG
#ifdef MOZ_SVG_RENDERER_LIBART
  NS_FreeSVGRendererLibartGlobals();
#endif
#ifdef MOZ_SVG_RENDERER_GDIPLUS
  NS_FreeSVGRendererGDIPlusGlobals();
#endif
#endif

  nsCSSFrameConstructor::ReleaseGlobals();
  nsTextTransformer::Shutdown();
  nsSpaceManager::Shutdown();
  nsImageFrame::ReleaseGlobals();

  nsCSSScanner::ReleaseGlobals();

  NS_IF_RELEASE(nsContentDLF::gUAStyleSheet);
  NS_IF_RELEASE(nsRuleNode::gLangService);
  nsGenericHTMLElement::Shutdown();

  nsAttrValue::Shutdown();
  nsContentUtils::Shutdown();
  nsLayoutStylesheetCache::Shutdown();
  NS_NameSpaceManagerShutdown();
  nsStyleSet::FreeGlobals();

  nsGlobalWindow::ShutDown();
  nsDOMClassInfo::ShutDown();
  nsTextControlFrame::ShutDown();
  nsXBLWindowKeyHandler::ShutDown();
  nsAutoCopyListener::Shutdown();
}

void
nsLayoutStatics::AddRef()
{
  NS_ASSERTION(sLayoutStaticRefcnt,
               "nsLayoutStatics already dropped to zero!");

  ++sLayoutStaticRefcnt;
  NS_LOG_ADDREF(&sLayoutStaticRefcnt, sLayoutStaticRefcnt,
                "nsLayoutStatics", 1);
}

void
nsLayoutStatics::Release()
{
  --sLayoutStaticRefcnt;
  NS_LOG_RELEASE(&sLayoutStaticRefcnt, sLayoutStaticRefcnt,
                 "nsLayoutStatics");

  if (!sLayoutStaticRefcnt)
    Shutdown();
}
