/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is IBM Corporation
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Author: Aaron Leventhal (aleventh@us.ibm.com)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsHTMLFormControlAccessibleWrap.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsIRadioControlElement.h"
#include "nsIRadioGroupContainer.H"
#include "nsTextFormatter.h"

// --------------------------------------------------------
// nsXULMenuAccessibleWrap Accessible
// --------------------------------------------------------

nsHTMLRadioButtonAccessibleWrap::nsHTMLRadioButtonAccessibleWrap(nsIDOMNode *aDOMNode, 
                                                         nsIWeakReference *aShell):
nsHTMLRadioButtonAccessible(aDOMNode, aShell)
{
}

NS_IMETHODIMP nsHTMLRadioButtonAccessibleWrap::GetDescription(nsAString& aDescription)
{
  aDescription.Truncate();

  nsCOMPtr<nsIRadioControlElement> radio(do_QueryInterface(mDOMNode));
  if (!radio) {
    return NS_ERROR_FAILURE;
  }

  NS_ASSERTION(radio, "We should only have HTML radio buttons here");
  nsCOMPtr<nsIRadioGroupContainer> radioGroupContainer =
    radio->GetRadioGroupContainer();

  if (radioGroupContainer) {
    nsCOMPtr<nsIDOMHTMLInputElement> input(do_QueryInterface(mDOMNode));
    PRInt32 radioIndex, radioItemsInGroup;
    if (NS_SUCCEEDED(radioGroupContainer->GetPositionInGroup(input, &radioIndex, 
                                                             &radioItemsInGroup))) {
      // Don't localize the string "of" -- that's just the format of this string.
      // The AT will parse the relevant numbers out and add its own localization.
      nsTextFormatter::ssprintf(aDescription, NS_LITERAL_STRING("%d of %d").get(),
                                radioIndex + 1, radioItemsInGroup);
      return NS_OK;
    }
  }
  return nsAccessibleWrap::GetDescription(aDescription);
}
