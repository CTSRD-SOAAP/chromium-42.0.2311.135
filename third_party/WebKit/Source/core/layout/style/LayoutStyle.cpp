/*
 * Copyright (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2011 Adobe Systems Incorporated. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "core/layout/style/LayoutStyle.h"

#include "core/css/resolver/StyleResolver.h"
#include "core/layout/LayoutTheme.h"
#include "core/layout/TextAutosizer.h"
#include "core/layout/style/AppliedTextDecoration.h"
#include "core/layout/style/BorderEdge.h"
#include "core/layout/style/ContentData.h"
#include "core/layout/style/DataEquivalency.h"
#include "core/layout/style/LayoutStyleConstants.h"
#include "core/layout/style/PathStyleMotionPath.h"
#include "core/layout/style/QuotesData.h"
#include "core/layout/style/ShadowList.h"
#include "core/layout/style/StyleImage.h"
#include "core/layout/style/StyleInheritedData.h"
#include "platform/LengthFunctions.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/fonts/Font.h"
#include "platform/fonts/FontSelector.h"
#include "platform/geometry/FloatRoundedRect.h"
#include "wtf/MathExtras.h"

#include <algorithm>

namespace blink {

struct SameSizeAsBorderValue {
    RGBA32 m_color;
    unsigned m_width;
};

static_assert(sizeof(BorderValue) == sizeof(SameSizeAsBorderValue), "BorderValue should stay small");

struct SameSizeAsLayoutStyle : public RefCounted<SameSizeAsLayoutStyle> {
    void* dataRefs[7];
    void* ownPtrs[1];
    void* dataRefSvgStyle;

    struct InheritedFlags {
        unsigned m_bitfields[2];
    } inherited_flags;

    struct NonInheritedFlags {
        unsigned m_bitfields[2];
    } noninherited_flags;
};

static_assert(sizeof(LayoutStyle) == sizeof(SameSizeAsLayoutStyle), "LayoutStyle should stay small");

inline LayoutStyle* defaultStyle()
{
    DEFINE_STATIC_REF(LayoutStyle, s_defaultStyle, (LayoutStyle::createDefaultStyle()));
    return s_defaultStyle;
}

PassRefPtr<LayoutStyle> LayoutStyle::create()
{
    return adoptRef(new LayoutStyle());
}

PassRefPtr<LayoutStyle> LayoutStyle::createDefaultStyle()
{
    return adoptRef(new LayoutStyle(DefaultStyle));
}

PassRefPtr<LayoutStyle> LayoutStyle::createAnonymousStyleWithDisplay(const LayoutStyle& parentStyle, EDisplay display)
{
    RefPtr<LayoutStyle> newStyle = LayoutStyle::create();
    newStyle->inheritFrom(parentStyle);
    newStyle->inheritUnicodeBidiFrom(parentStyle);
    newStyle->setDisplay(display);
    return newStyle;
}

PassRefPtr<LayoutStyle> LayoutStyle::clone(const LayoutStyle& other)
{
    return adoptRef(new LayoutStyle(other));
}

ALWAYS_INLINE LayoutStyle::LayoutStyle()
    : m_box(defaultStyle()->m_box)
    , visual(defaultStyle()->visual)
    , m_background(defaultStyle()->m_background)
    , surround(defaultStyle()->surround)
    , rareNonInheritedData(defaultStyle()->rareNonInheritedData)
    , rareInheritedData(defaultStyle()->rareInheritedData)
    , inherited(defaultStyle()->inherited)
    , m_svgStyle(defaultStyle()->m_svgStyle)
{
    setBitDefaults(); // Would it be faster to copy this from the default style?
    static_assert((sizeof(InheritedFlags) <= 8), "InheritedFlags should not grow");
    static_assert((sizeof(NonInheritedFlags) <= 8), "NonInheritedFlags should not grow");
}

ALWAYS_INLINE LayoutStyle::LayoutStyle(DefaultStyleTag)
{
    setBitDefaults();

    m_box.init();
    visual.init();
    m_background.init();
    surround.init();
    rareNonInheritedData.init();
    rareNonInheritedData.access()->m_deprecatedFlexibleBox.init();
    rareNonInheritedData.access()->m_flexibleBox.init();
    rareNonInheritedData.access()->m_multiCol.init();
    rareNonInheritedData.access()->m_transform.init();
    rareNonInheritedData.access()->m_willChange.init();
    rareNonInheritedData.access()->m_filter.init();
    rareNonInheritedData.access()->m_grid.init();
    rareNonInheritedData.access()->m_gridItem.init();
    rareInheritedData.init();
    inherited.init();
    m_svgStyle.init();
}

ALWAYS_INLINE LayoutStyle::LayoutStyle(const LayoutStyle& o)
    : RefCounted<LayoutStyle>()
    , m_box(o.m_box)
    , visual(o.visual)
    , m_background(o.m_background)
    , surround(o.surround)
    , rareNonInheritedData(o.rareNonInheritedData)
    , rareInheritedData(o.rareInheritedData)
    , inherited(o.inherited)
    , m_svgStyle(o.m_svgStyle)
    , inherited_flags(o.inherited_flags)
    , noninherited_flags(o.noninherited_flags)
{
}

static StyleRecalcChange diffPseudoStyles(const LayoutStyle& oldStyle, const LayoutStyle& newStyle)
{
    // If the pseudoStyles have changed, we want any StyleRecalcChange that is not NoChange
    // because setStyle will do the right thing with anything else.
    if (!oldStyle.hasAnyPublicPseudoStyles())
        return NoChange;
    for (PseudoId pseudoId = FIRST_PUBLIC_PSEUDOID; pseudoId < FIRST_INTERNAL_PSEUDOID; pseudoId = static_cast<PseudoId>(pseudoId + 1)) {
        if (!oldStyle.hasPseudoStyle(pseudoId))
            continue;
        const LayoutStyle* newPseudoStyle = newStyle.getCachedPseudoStyle(pseudoId);
        if (!newPseudoStyle)
            return NoInherit;
        const LayoutStyle* oldPseudoStyle = oldStyle.getCachedPseudoStyle(pseudoId);
        if (oldPseudoStyle && *oldPseudoStyle != *newPseudoStyle)
            return NoInherit;
    }
    return NoChange;
}

StyleRecalcChange LayoutStyle::stylePropagationDiff(const LayoutStyle* oldStyle, const LayoutStyle* newStyle)
{
    if ((!oldStyle && newStyle) || (oldStyle && !newStyle))
        return Reattach;

    if (!oldStyle && !newStyle)
        return NoChange;

    if (oldStyle->display() != newStyle->display()
        || oldStyle->hasPseudoStyle(FIRST_LETTER) != newStyle->hasPseudoStyle(FIRST_LETTER)
        || oldStyle->columnSpan() != newStyle->columnSpan()
        || !oldStyle->contentDataEquivalent(newStyle)
        || oldStyle->hasTextCombine() != newStyle->hasTextCombine()
        || oldStyle->justifyItems() != newStyle->justifyItems()
        || oldStyle->alignItems() != newStyle->alignItems())
        return Reattach;

    if (oldStyle->inheritedNotEqual(*newStyle)
        || oldStyle->hasExplicitlyInheritedProperties()
        || newStyle->hasExplicitlyInheritedProperties())
        return Inherit;

    if (*oldStyle == *newStyle)
        return diffPseudoStyles(*oldStyle, *newStyle);

    return NoInherit;
}

ItemPosition LayoutStyle::resolveAlignment(const LayoutStyle& parentStyle, const LayoutStyle& childStyle, ItemPosition resolvedAutoPositionForRenderer)
{
    // The auto keyword computes to the parent's align-items computed value, or to "stretch", if not set or "auto".
    if (childStyle.alignSelf() == ItemPositionAuto)
        return (parentStyle.alignItems() == ItemPositionAuto) ? resolvedAutoPositionForRenderer : parentStyle.alignItems();
    return childStyle.alignSelf();
}

ItemPosition LayoutStyle::resolveJustification(const LayoutStyle& parentStyle, const LayoutStyle& childStyle, ItemPosition resolvedAutoPositionForRenderer)
{
    if (childStyle.justifySelf() == ItemPositionAuto)
        return (parentStyle.justifyItems() == ItemPositionAuto) ? resolvedAutoPositionForRenderer : parentStyle.justifyItems();
    return childStyle.justifySelf();
}

void LayoutStyle::inheritFrom(const LayoutStyle& inheritParent, IsAtShadowBoundary isAtShadowBoundary)
{
    if (isAtShadowBoundary == AtShadowBoundary) {
        // Even if surrounding content is user-editable, shadow DOM should act as a single unit, and not necessarily be editable
        EUserModify currentUserModify = userModify();
        rareInheritedData = inheritParent.rareInheritedData;
        setUserModify(currentUserModify);
    } else {
        rareInheritedData = inheritParent.rareInheritedData;
    }
    inherited = inheritParent.inherited;
    inherited_flags = inheritParent.inherited_flags;
    if (m_svgStyle != inheritParent.m_svgStyle)
        m_svgStyle.access()->inheritFrom(inheritParent.m_svgStyle.get());
}

void LayoutStyle::copyNonInheritedFrom(const LayoutStyle& other)
{
    m_box = other.m_box;
    visual = other.visual;
    m_background = other.m_background;
    surround = other.surround;
    rareNonInheritedData = other.rareNonInheritedData;
    // The flags are copied one-by-one because noninherited_flags contains a bunch of stuff other than real style data.
    noninherited_flags.effectiveDisplay = other.noninherited_flags.effectiveDisplay;
    noninherited_flags.originalDisplay = other.noninherited_flags.originalDisplay;
    noninherited_flags.overflowX = other.noninherited_flags.overflowX;
    noninherited_flags.overflowY = other.noninherited_flags.overflowY;
    noninherited_flags.verticalAlign = other.noninherited_flags.verticalAlign;
    noninherited_flags.clear = other.noninherited_flags.clear;
    noninherited_flags.position = other.noninherited_flags.position;
    noninherited_flags.floating = other.noninherited_flags.floating;
    noninherited_flags.tableLayout = other.noninherited_flags.tableLayout;
    noninherited_flags.unicodeBidi = other.noninherited_flags.unicodeBidi;
    noninherited_flags.pageBreakBefore = other.noninherited_flags.pageBreakBefore;
    noninherited_flags.pageBreakAfter = other.noninherited_flags.pageBreakAfter;
    noninherited_flags.pageBreakInside = other.noninherited_flags.pageBreakInside;
    noninherited_flags.explicitInheritance = other.noninherited_flags.explicitInheritance;
    noninherited_flags.hasViewportUnits = other.noninherited_flags.hasViewportUnits;
    if (m_svgStyle != other.m_svgStyle)
        m_svgStyle.access()->copyNonInheritedFrom(other.m_svgStyle.get());
    ASSERT(zoom() == initialZoom());
}

bool LayoutStyle::operator==(const LayoutStyle& o) const
{
    // compare everything except the pseudoStyle pointer
    return inherited_flags == o.inherited_flags
        && noninherited_flags == o.noninherited_flags
        && m_box == o.m_box
        && visual == o.visual
        && m_background == o.m_background
        && surround == o.surround
        && rareNonInheritedData == o.rareNonInheritedData
        && rareInheritedData == o.rareInheritedData
        && inherited == o.inherited
        && m_svgStyle == o.m_svgStyle;
}

bool LayoutStyle::isStyleAvailable() const
{
    return this != StyleResolver::styleNotYetAvailable();
}

bool LayoutStyle::hasUniquePseudoStyle() const
{
    if (!m_cachedPseudoStyles || styleType() != NOPSEUDO)
        return false;

    for (size_t i = 0; i < m_cachedPseudoStyles->size(); ++i) {
        const LayoutStyle& pseudoStyle = *m_cachedPseudoStyles->at(i);
        if (pseudoStyle.unique())
            return true;
    }

    return false;
}

LayoutStyle* LayoutStyle::getCachedPseudoStyle(PseudoId pid) const
{
    if (!m_cachedPseudoStyles || !m_cachedPseudoStyles->size())
        return 0;

    if (styleType() != NOPSEUDO)
        return 0;

    for (size_t i = 0; i < m_cachedPseudoStyles->size(); ++i) {
        LayoutStyle* pseudoStyle = m_cachedPseudoStyles->at(i).get();
        if (pseudoStyle->styleType() == pid)
            return pseudoStyle;
    }

    return 0;
}

LayoutStyle* LayoutStyle::addCachedPseudoStyle(PassRefPtr<LayoutStyle> pseudo)
{
    if (!pseudo)
        return 0;

    ASSERT(pseudo->styleType() > NOPSEUDO);

    LayoutStyle* result = pseudo.get();

    if (!m_cachedPseudoStyles)
        m_cachedPseudoStyles = adoptPtr(new PseudoStyleCache);

    m_cachedPseudoStyles->append(pseudo);

    return result;
}

void LayoutStyle::removeCachedPseudoStyle(PseudoId pid)
{
    if (!m_cachedPseudoStyles)
        return;
    for (size_t i = 0; i < m_cachedPseudoStyles->size(); ++i) {
        LayoutStyle* pseudoStyle = m_cachedPseudoStyles->at(i).get();
        if (pseudoStyle->styleType() == pid) {
            m_cachedPseudoStyles->remove(i);
            return;
        }
    }
}

bool LayoutStyle::inheritedNotEqual(const LayoutStyle& other) const
{
    return inherited_flags != other.inherited_flags
        || inherited != other.inherited
        || font().loadingCustomFonts() != other.font().loadingCustomFonts()
        || m_svgStyle->inheritedNotEqual(other.m_svgStyle.get())
        || rareInheritedData != other.rareInheritedData;
}

bool LayoutStyle::inheritedDataShared(const LayoutStyle& other) const
{
    // This is a fast check that only looks if the data structures are shared.
    return inherited_flags == other.inherited_flags
        && inherited.get() == other.inherited.get()
        && m_svgStyle.get() == other.m_svgStyle.get()
        && rareInheritedData.get() == other.rareInheritedData.get();
}

static bool dependenceOnContentHeightHasChanged(const LayoutStyle& a, const LayoutStyle& b)
{
    // If top or bottom become auto/non-auto then it means we either have to solve height based
    // on the content or stop doing so (http://www.w3.org/TR/CSS2/visudet.html#abs-non-replaced-height)
    // - either way requires a layout.
    return a.logicalTop().isAuto() != b.logicalTop().isAuto() || a.logicalBottom().isAuto() != b.logicalBottom().isAuto();
}

StyleDifference LayoutStyle::visualInvalidationDiff(const LayoutStyle& other) const
{
    // Note, we use .get() on each DataRef below because DataRef::operator== will do a deep
    // compare, which is duplicate work when we're going to compare each property inside
    // this function anyway.

    StyleDifference diff;
    if (m_svgStyle.get() != other.m_svgStyle.get())
        diff = m_svgStyle->diff(other.m_svgStyle.get());

    if ((!diff.needsFullLayout() || !diff.needsPaintInvalidation()) && diffNeedsFullLayoutAndPaintInvalidation(other)) {
        diff.setNeedsFullLayout();
        diff.setNeedsPaintInvalidationObject();
    }

    if (!diff.needsFullLayout() && diffNeedsFullLayout(other))
        diff.setNeedsFullLayout();

    if (!diff.needsFullLayout() && surround->margin != other.surround->margin) {
        // Relative-positioned elements collapse their margins so need a full layout.
        if (position() == AbsolutePosition || position() == FixedPosition)
            diff.setNeedsPositionedMovementLayout();
        else
            diff.setNeedsFullLayout();
    }

    if (!diff.needsFullLayout() && position() != StaticPosition && surround->offset != other.surround->offset) {
        // Optimize for the case where a positioned layer is moving but not changing size.
        if (dependenceOnContentHeightHasChanged(*this, other))
            diff.setNeedsFullLayout();
        else
            diff.setNeedsPositionedMovementLayout();
    }

    if (diffNeedsPaintInvalidationLayer(other))
        diff.setNeedsPaintInvalidationLayer();
    else if (diffNeedsPaintInvalidationObject(other))
        diff.setNeedsPaintInvalidationObject();

    updatePropertySpecificDifferences(other, diff);

    // Cursors are not checked, since they will be set appropriately in response to mouse events,
    // so they don't need to cause any paint invalidation or layout.

    // Animations don't need to be checked either. We always set the new style on the LayoutObject, so we will get a chance to fire off
    // the resulting transition properly.

    return diff;
}

bool LayoutStyle::diffNeedsFullLayoutAndPaintInvalidation(const LayoutStyle& other) const
{
    // FIXME: Not all cases in this method need both full layout and paint invalidation.
    // Should move cases into diffNeedsFullLayout() if
    // - don't need paint invalidation at all;
    // - or the renderer knows how to exactly invalidate paints caused by the layout change
    //   instead of forced full paint invalidation.

    if (surround.get() != other.surround.get()) {
        // If our border widths change, then we need to layout. Other changes to borders only necessitate a paint invalidation.
        if (borderLeftWidth() != other.borderLeftWidth()
            || borderTopWidth() != other.borderTopWidth()
            || borderBottomWidth() != other.borderBottomWidth()
            || borderRightWidth() != other.borderRightWidth())
            return true;
    }

    if (rareNonInheritedData.get() != other.rareNonInheritedData.get()) {
        if (rareNonInheritedData->m_appearance != other.rareNonInheritedData->m_appearance
            || rareNonInheritedData->marginBeforeCollapse != other.rareNonInheritedData->marginBeforeCollapse
            || rareNonInheritedData->marginAfterCollapse != other.rareNonInheritedData->marginAfterCollapse
            || rareNonInheritedData->lineClamp != other.rareNonInheritedData->lineClamp
            || rareNonInheritedData->textOverflow != other.rareNonInheritedData->textOverflow
            || rareNonInheritedData->m_wrapFlow != other.rareNonInheritedData->m_wrapFlow
            || rareNonInheritedData->m_wrapThrough != other.rareNonInheritedData->m_wrapThrough
            || rareNonInheritedData->m_shapeMargin != other.rareNonInheritedData->m_shapeMargin
            || rareNonInheritedData->m_order != other.rareNonInheritedData->m_order
            || rareNonInheritedData->m_justifyContent != other.rareNonInheritedData->m_justifyContent
            || rareNonInheritedData->m_grid.get() != other.rareNonInheritedData->m_grid.get()
            || rareNonInheritedData->m_gridItem.get() != other.rareNonInheritedData->m_gridItem.get()
            || rareNonInheritedData->m_textCombine != other.rareNonInheritedData->m_textCombine
            || rareNonInheritedData->hasFilters() != other.rareNonInheritedData->hasFilters())
            return true;

        if (rareNonInheritedData->m_deprecatedFlexibleBox.get() != other.rareNonInheritedData->m_deprecatedFlexibleBox.get()
            && *rareNonInheritedData->m_deprecatedFlexibleBox.get() != *other.rareNonInheritedData->m_deprecatedFlexibleBox.get())
            return true;

        if (rareNonInheritedData->m_flexibleBox.get() != other.rareNonInheritedData->m_flexibleBox.get()
            && *rareNonInheritedData->m_flexibleBox.get() != *other.rareNonInheritedData->m_flexibleBox.get())
            return true;

        // FIXME: We should add an optimized form of layout that just recomputes visual overflow.
        if (!rareNonInheritedData->shadowDataEquivalent(*other.rareNonInheritedData.get()))
            return true;

        if (!rareNonInheritedData->reflectionDataEquivalent(*other.rareNonInheritedData.get()))
            return true;

        if (rareNonInheritedData->m_multiCol.get() != other.rareNonInheritedData->m_multiCol.get()
            && *rareNonInheritedData->m_multiCol.get() != *other.rareNonInheritedData->m_multiCol.get())
            return true;

        // If the counter directives change, trigger a relayout to re-calculate counter values and rebuild the counter node tree.
        const CounterDirectiveMap* mapA = rareNonInheritedData->m_counterDirectives.get();
        const CounterDirectiveMap* mapB = other.rareNonInheritedData->m_counterDirectives.get();
        if (!(mapA == mapB || (mapA && mapB && *mapA == *mapB)))
            return true;

        // We only need do layout for opacity changes if adding or losing opacity could trigger a change
        // in us being a stacking context.
        if (hasAutoZIndex() != other.hasAutoZIndex() && rareNonInheritedData->hasOpacity() != other.rareNonInheritedData->hasOpacity()) {
            // FIXME: We would like to use SimplifiedLayout here, but we can't quite do that yet.
            // We need to make sure SimplifiedLayout can operate correctly on RenderInlines (we will need
            // to add a selfNeedsSimplifiedLayout bit in order to not get confused and taint every line).
            // In addition we need to solve the floating object issue when layers come and go. Right now
            // a full layout is necessary to keep floating object lists sane.
            return true;
        }
    }

    if (rareInheritedData.get() != other.rareInheritedData.get()) {
        if (rareInheritedData->highlight != other.rareInheritedData->highlight
            || rareInheritedData->indent != other.rareInheritedData->indent
            || rareInheritedData->m_textAlignLast != other.rareInheritedData->m_textAlignLast
            || rareInheritedData->m_textIndentLine != other.rareInheritedData->m_textIndentLine
            || rareInheritedData->m_effectiveZoom != other.rareInheritedData->m_effectiveZoom
            || rareInheritedData->wordBreak != other.rareInheritedData->wordBreak
            || rareInheritedData->overflowWrap != other.rareInheritedData->overflowWrap
            || rareInheritedData->lineBreak != other.rareInheritedData->lineBreak
            || rareInheritedData->textSecurity != other.rareInheritedData->textSecurity
            || rareInheritedData->hyphens != other.rareInheritedData->hyphens
            || rareInheritedData->hyphenationLimitBefore != other.rareInheritedData->hyphenationLimitBefore
            || rareInheritedData->hyphenationLimitAfter != other.rareInheritedData->hyphenationLimitAfter
            || rareInheritedData->hyphenationString != other.rareInheritedData->hyphenationString
            || rareInheritedData->locale != other.rareInheritedData->locale
            || rareInheritedData->m_rubyPosition != other.rareInheritedData->m_rubyPosition
            || rareInheritedData->textEmphasisMark != other.rareInheritedData->textEmphasisMark
            || rareInheritedData->textEmphasisPosition != other.rareInheritedData->textEmphasisPosition
            || rareInheritedData->textEmphasisCustomMark != other.rareInheritedData->textEmphasisCustomMark
            || rareInheritedData->m_textJustify != other.rareInheritedData->m_textJustify
            || rareInheritedData->m_textOrientation != other.rareInheritedData->m_textOrientation
            || rareInheritedData->m_tabSize != other.rareInheritedData->m_tabSize
            || rareInheritedData->m_lineBoxContain != other.rareInheritedData->m_lineBoxContain
            || rareInheritedData->listStyleImage != other.rareInheritedData->listStyleImage
            || rareInheritedData->textStrokeWidth != other.rareInheritedData->textStrokeWidth)
            return true;

        if (!rareInheritedData->shadowDataEquivalent(*other.rareInheritedData.get()))
            return true;

        if (!rareInheritedData->quotesDataEquivalent(*other.rareInheritedData.get()))
            return true;
    }

    if (inherited->textAutosizingMultiplier != other.inherited->textAutosizingMultiplier)
        return true;

    if (inherited->font.loadingCustomFonts() != other.inherited->font.loadingCustomFonts())
        return true;

    if (inherited.get() != other.inherited.get()) {
        if (inherited->line_height != other.inherited->line_height
            || inherited->font != other.inherited->font
            || inherited->horizontal_border_spacing != other.inherited->horizontal_border_spacing
            || inherited->vertical_border_spacing != other.inherited->vertical_border_spacing)
            return true;
    }

    if (inherited_flags._box_direction != other.inherited_flags._box_direction
        || inherited_flags.m_rtlOrdering != other.inherited_flags.m_rtlOrdering
        || inherited_flags._text_align != other.inherited_flags._text_align
        || inherited_flags._text_transform != other.inherited_flags._text_transform
        || inherited_flags._direction != other.inherited_flags._direction
        || inherited_flags._white_space != other.inherited_flags._white_space
        || inherited_flags.m_writingMode != other.inherited_flags.m_writingMode)
        return true;

    if (noninherited_flags.overflowX != other.noninherited_flags.overflowX
        || noninherited_flags.overflowY != other.noninherited_flags.overflowY
        || noninherited_flags.clear != other.noninherited_flags.clear
        || noninherited_flags.unicodeBidi != other.noninherited_flags.unicodeBidi
        || noninherited_flags.floating != other.noninherited_flags.floating
        || noninherited_flags.originalDisplay != other.noninherited_flags.originalDisplay)
        return true;

    if (noninherited_flags.effectiveDisplay >= FIRST_TABLE_DISPLAY && noninherited_flags.effectiveDisplay <= LAST_TABLE_DISPLAY) {
        if (inherited_flags._border_collapse != other.inherited_flags._border_collapse
            || inherited_flags._empty_cells != other.inherited_flags._empty_cells
            || inherited_flags._caption_side != other.inherited_flags._caption_side
            || noninherited_flags.tableLayout != other.noninherited_flags.tableLayout)
            return true;

        // In the collapsing border model, 'hidden' suppresses other borders, while 'none'
        // does not, so these style differences can be width differences.
        if (inherited_flags._border_collapse
            && ((borderTopStyle() == BHIDDEN && other.borderTopStyle() == BNONE)
                || (borderTopStyle() == BNONE && other.borderTopStyle() == BHIDDEN)
                || (borderBottomStyle() == BHIDDEN && other.borderBottomStyle() == BNONE)
                || (borderBottomStyle() == BNONE && other.borderBottomStyle() == BHIDDEN)
                || (borderLeftStyle() == BHIDDEN && other.borderLeftStyle() == BNONE)
                || (borderLeftStyle() == BNONE && other.borderLeftStyle() == BHIDDEN)
                || (borderRightStyle() == BHIDDEN && other.borderRightStyle() == BNONE)
                || (borderRightStyle() == BNONE && other.borderRightStyle() == BHIDDEN)))
            return true;
    } else if (noninherited_flags.effectiveDisplay == LIST_ITEM) {
        if (inherited_flags._list_style_type != other.inherited_flags._list_style_type
            || inherited_flags._list_style_position != other.inherited_flags._list_style_position)
            return true;
    }

    if ((visibility() == COLLAPSE) != (other.visibility() == COLLAPSE))
        return true;

    if (!m_background->outline().visuallyEqual(other.m_background->outline())) {
        // FIXME: We only really need to recompute the overflow but we don't have an optimized layout for it.
        return true;
    }

    if (hasPseudoStyle(SCROLLBAR) != other.hasPseudoStyle(SCROLLBAR))
        return true;

    // Movement of non-static-positioned object is special cased in LayoutStyle::visualInvalidationDiff().

    return false;
}

bool LayoutStyle::diffNeedsFullLayout(const LayoutStyle& other) const
{
    if (m_box.get() != other.m_box.get()) {
        if (m_box->width() != other.m_box->width()
            || m_box->minWidth() != other.m_box->minWidth()
            || m_box->maxWidth() != other.m_box->maxWidth()
            || m_box->height() != other.m_box->height()
            || m_box->minHeight() != other.m_box->minHeight()
            || m_box->maxHeight() != other.m_box->maxHeight())
            return true;

        if (m_box->verticalAlign() != other.m_box->verticalAlign())
            return true;

        if (m_box->boxSizing() != other.m_box->boxSizing())
            return true;
    }

    if (noninherited_flags.verticalAlign != other.noninherited_flags.verticalAlign
        || noninherited_flags.position != other.noninherited_flags.position)
        return true;

    if (surround.get() != other.surround.get()) {
        if (surround->padding != other.surround->padding)
            return true;
    }

    if (rareNonInheritedData.get() != other.rareNonInheritedData.get()) {
        if (rareNonInheritedData->m_alignContent != other.rareNonInheritedData->m_alignContent
            || rareNonInheritedData->m_alignContentDistribution != other.rareNonInheritedData->m_alignContentDistribution
            || rareNonInheritedData->m_alignItems != other.rareNonInheritedData->m_alignItems
            || rareNonInheritedData->m_alignSelf != other.rareNonInheritedData->m_alignSelf)
            return true;
    }

    return false;
}

bool LayoutStyle::diffNeedsPaintInvalidationLayer(const LayoutStyle& other) const
{
    if (position() != StaticPosition && (visual->clip != other.visual->clip || visual->hasAutoClip != other.visual->hasAutoClip))
        return true;

    if (rareNonInheritedData.get() != other.rareNonInheritedData.get()) {
        if (RuntimeEnabledFeatures::cssCompositingEnabled()
            && (rareNonInheritedData->m_effectiveBlendMode != other.rareNonInheritedData->m_effectiveBlendMode
                || rareNonInheritedData->m_isolation != other.rareNonInheritedData->m_isolation))
            return true;

        if (rareNonInheritedData->m_mask != other.rareNonInheritedData->m_mask
            || rareNonInheritedData->m_maskBoxImage != other.rareNonInheritedData->m_maskBoxImage)
            return true;
    }

    return false;
}

bool LayoutStyle::diffNeedsPaintInvalidationObject(const LayoutStyle& other) const
{
    if (inherited_flags._visibility != other.inherited_flags._visibility
        || inherited_flags.m_printColorAdjust != other.inherited_flags.m_printColorAdjust
        || inherited_flags._insideLink != other.inherited_flags._insideLink
        || !surround->border.visuallyEqual(other.surround->border)
        || !m_background->visuallyEqual(*other.m_background))
        return true;

    if (rareInheritedData.get() != other.rareInheritedData.get()) {
        if (rareInheritedData->userModify != other.rareInheritedData->userModify
            || rareInheritedData->userSelect != other.rareInheritedData->userSelect
            || rareInheritedData->m_imageRendering != other.rareInheritedData->m_imageRendering)
            return true;
    }

    if (rareNonInheritedData.get() != other.rareNonInheritedData.get()) {
        if (rareNonInheritedData->userDrag != other.rareNonInheritedData->userDrag
            || rareNonInheritedData->m_objectFit != other.rareNonInheritedData->m_objectFit
            || rareNonInheritedData->m_objectPosition != other.rareNonInheritedData->m_objectPosition
            || !rareNonInheritedData->shapeOutsideDataEquivalent(*other.rareNonInheritedData.get())
            || !rareNonInheritedData->clipPathDataEquivalent(*other.rareNonInheritedData.get())
            || (visitedLinkBorderLeftColor() != other.visitedLinkBorderLeftColor() && borderLeftWidth())
            || (visitedLinkBorderRightColor() != other.visitedLinkBorderRightColor() && borderRightWidth())
            || (visitedLinkBorderBottomColor() != other.visitedLinkBorderBottomColor() && borderBottomWidth())
            || (visitedLinkBorderTopColor() != other.visitedLinkBorderTopColor() && borderTopWidth())
            || (visitedLinkOutlineColor() != other.visitedLinkOutlineColor() && outlineWidth())
            || (visitedLinkBackgroundColor() != other.visitedLinkBackgroundColor()))
            return true;
    }

    if (resize() != other.resize())
        return true;

    return false;
}

void LayoutStyle::updatePropertySpecificDifferences(const LayoutStyle& other, StyleDifference& diff) const
{
    // StyleAdjuster has ensured that zIndex is non-auto only if it's applicable.
    if (m_box->zIndex() != other.m_box->zIndex() || m_box->hasAutoZIndex() != other.m_box->hasAutoZIndex())
        diff.setZIndexChanged();

    if (rareNonInheritedData.get() != other.rareNonInheritedData.get()) {
        if (!transformDataEquivalent(other))
            diff.setTransformChanged();

        if (rareNonInheritedData->opacity != other.rareNonInheritedData->opacity)
            diff.setOpacityChanged();

        if (rareNonInheritedData->m_filter != other.rareNonInheritedData->m_filter)
            diff.setFilterChanged();
    }

    if (!diff.needsPaintInvalidation()) {
        if (inherited->color != other.inherited->color
            || inherited->visitedLinkColor != other.inherited->visitedLinkColor
            || inherited_flags.m_textUnderline != other.inherited_flags.m_textUnderline
            || visual->textDecoration != other.visual->textDecoration) {
            diff.setTextOrColorChanged();
        } else if (rareNonInheritedData.get() != other.rareNonInheritedData.get()) {
            if (rareNonInheritedData->m_textDecorationStyle != other.rareNonInheritedData->m_textDecorationStyle
                || rareNonInheritedData->m_textDecorationColor != other.rareNonInheritedData->m_textDecorationColor
                || rareNonInheritedData->m_visitedLinkTextDecorationColor != other.rareNonInheritedData->m_visitedLinkTextDecorationColor)
                diff.setTextOrColorChanged();
        } else if (rareInheritedData.get() != other.rareInheritedData.get()) {
            if (rareInheritedData->textFillColor() != other.rareInheritedData->textFillColor()
                || rareInheritedData->textStrokeColor() != other.rareInheritedData->textStrokeColor()
                || rareInheritedData->textEmphasisColor() != other.rareInheritedData->textEmphasisColor()
                || rareInheritedData->visitedLinkTextFillColor() != other.rareInheritedData->visitedLinkTextFillColor()
                || rareInheritedData->visitedLinkTextStrokeColor() != other.rareInheritedData->visitedLinkTextStrokeColor()
                || rareInheritedData->visitedLinkTextEmphasisColor() != other.rareInheritedData->visitedLinkTextEmphasisColor()
                || rareInheritedData->textEmphasisFill != other.rareInheritedData->textEmphasisFill
                || rareInheritedData->appliedTextDecorations != other.rareInheritedData->appliedTextDecorations)
                diff.setTextOrColorChanged();
        }
    }
}

void LayoutStyle::addCursor(PassRefPtr<StyleImage> image, bool hotSpotSpecified, const IntPoint& hotSpot)
{
    if (!rareInheritedData.access()->cursorData)
        rareInheritedData.access()->cursorData = CursorList::create();
    rareInheritedData.access()->cursorData->append(CursorData(image, hotSpotSpecified, hotSpot));
}

void LayoutStyle::setCursorList(PassRefPtr<CursorList> other)
{
    rareInheritedData.access()->cursorData = other;
}

void LayoutStyle::setQuotes(PassRefPtr<QuotesData> q)
{
    rareInheritedData.access()->quotes = q;
}

void LayoutStyle::clearCursorList()
{
    if (rareInheritedData->cursorData)
        rareInheritedData.access()->cursorData = nullptr;
}

void LayoutStyle::addCallbackSelector(const String& selector)
{
    if (!rareNonInheritedData->m_callbackSelectors.contains(selector))
        rareNonInheritedData.access()->m_callbackSelectors.append(selector);
}

void LayoutStyle::clearContent()
{
    if (rareNonInheritedData->m_content)
        rareNonInheritedData.access()->m_content = nullptr;
}

void LayoutStyle::appendContent(PassOwnPtr<ContentData> contentData)
{
    OwnPtr<ContentData>& content = rareNonInheritedData.access()->m_content;
    ContentData* lastContent = content.get();
    while (lastContent && lastContent->next())
        lastContent = lastContent->next();

    if (lastContent)
        lastContent->setNext(contentData);
    else
        content = contentData;
}

void LayoutStyle::setContent(PassRefPtr<StyleImage> image, bool add)
{
    if (!image)
        return;

    if (add) {
        appendContent(ContentData::create(image));
        return;
    }

    rareNonInheritedData.access()->m_content = ContentData::create(image);
}

void LayoutStyle::setContent(const String& string, bool add)
{
    OwnPtr<ContentData>& content = rareNonInheritedData.access()->m_content;
    if (add) {
        ContentData* lastContent = content.get();
        while (lastContent && lastContent->next())
            lastContent = lastContent->next();

        if (lastContent) {
            // We attempt to merge with the last ContentData if possible.
            if (lastContent->isText()) {
                TextContentData* textContent = toTextContentData(lastContent);
                textContent->setText(textContent->text() + string);
            } else {
                lastContent->setNext(ContentData::create(string));
            }

            return;
        }
    }

    content = ContentData::create(string);
}

void LayoutStyle::setContent(PassOwnPtr<CounterContent> counter, bool add)
{
    if (!counter)
        return;

    if (add) {
        appendContent(ContentData::create(counter));
        return;
    }

    rareNonInheritedData.access()->m_content = ContentData::create(counter);
}

void LayoutStyle::setContent(QuoteType quote, bool add)
{
    if (add) {
        appendContent(ContentData::create(quote));
        return;
    }

    rareNonInheritedData.access()->m_content = ContentData::create(quote);
}

bool LayoutStyle::hasWillChangeCompositingHint() const
{
    for (size_t i = 0; i < rareNonInheritedData->m_willChange->m_properties.size(); ++i) {
        switch (rareNonInheritedData->m_willChange->m_properties[i]) {
        case CSSPropertyOpacity:
        case CSSPropertyTransform:
        case CSSPropertyWebkitTransform:
        case CSSPropertyTop:
        case CSSPropertyLeft:
        case CSSPropertyBottom:
        case CSSPropertyRight:
            return true;
        default:
            break;
        }
    }
    return false;
}

inline bool requireTransformOrigin(const Vector<RefPtr<TransformOperation>>& transformOperations, LayoutStyle::ApplyTransformOrigin applyOrigin, LayoutStyle::ApplyMotionPath applyMotionPath)
{
    // transform-origin brackets the transform with translate operations.
    // Optimize for the case where the only transform is a translation, since the transform-origin is irrelevant
    // in that case.
    if (applyOrigin != LayoutStyle::IncludeTransformOrigin)
        return false;

    if (applyMotionPath == LayoutStyle::IncludeMotionPath)
        return true;

    unsigned size = transformOperations.size();
    for (unsigned i = 0; i < size; ++i) {
        TransformOperation::OperationType type = transformOperations[i]->type();
        if (type != TransformOperation::TranslateX
            && type != TransformOperation::TranslateY
            && type != TransformOperation::Translate
            && type != TransformOperation::TranslateZ
            && type != TransformOperation::Translate3D)
            return true;
    }

    return false;
}

void LayoutStyle::applyTransform(TransformationMatrix& transform, const LayoutSize& borderBoxSize, ApplyTransformOrigin applyOrigin, ApplyMotionPath applyMotionPath) const
{
    applyTransform(transform, FloatRect(FloatPoint(), FloatSize(borderBoxSize)), applyOrigin, applyMotionPath);
}

void LayoutStyle::applyTransform(TransformationMatrix& transform, const FloatRect& boundingBox, ApplyTransformOrigin applyOrigin, ApplyMotionPath applyMotionPath) const
{
    if (!hasMotionPath())
        applyMotionPath = ExcludeMotionPath;
    const Vector<RefPtr<TransformOperation>>& transformOperations = rareNonInheritedData->m_transform->m_operations.operations();
    bool applyTransformOrigin = requireTransformOrigin(transformOperations, applyOrigin, applyMotionPath);

    float offsetX = transformOriginX().type() == Percent ? boundingBox.x() : 0;
    float offsetY = transformOriginY().type() == Percent ? boundingBox.y() : 0;

    if (applyTransformOrigin) {
        transform.translate3d(floatValueForLength(transformOriginX(), boundingBox.width()) + offsetX,
            floatValueForLength(transformOriginY(), boundingBox.height()) + offsetY,
            transformOriginZ());
    }

    if (applyMotionPath == LayoutStyle::IncludeMotionPath)
        applyMotionPathTransform(transform);

    unsigned size = transformOperations.size();
    for (unsigned i = 0; i < size; ++i)
        transformOperations[i]->apply(transform, boundingBox.size());

    if (applyTransformOrigin) {
        transform.translate3d(-floatValueForLength(transformOriginX(), boundingBox.width()) - offsetX,
            -floatValueForLength(transformOriginY(), boundingBox.height()) - offsetY,
            -transformOriginZ());
    }
}

void LayoutStyle::applyMotionPathTransform(TransformationMatrix& transform) const
{
    const StyleMotionData& motionData = rareNonInheritedData->m_transform->m_motion;
    ASSERT(motionData.m_path && motionData.m_path->isPathStyleMotionPath());
    const PathStyleMotionPath& motionPath = toPathStyleMotionPath(*motionData.m_path);
    float pathLength = motionPath.length();
    float length = clampTo<float>(floatValueForLength(motionData.m_position, pathLength), 0, pathLength);

    FloatPoint point;
    float angle;
    if (!motionPath.path().pointAndNormalAtLength(length, point, angle))
        return;
    if (motionData.m_rotationType == MotionRotationFixed)
        angle = 0;

    transform.translate(point.x(), point.y());
    transform.rotate(angle + motionData.m_rotation);
}

void LayoutStyle::setTextShadow(PassRefPtr<ShadowList> s)
{
    rareInheritedData.access()->textShadow = s;
}

void LayoutStyle::setBoxShadow(PassRefPtr<ShadowList> s)
{
    rareNonInheritedData.access()->m_boxShadow = s;
}

static FloatRoundedRect::Radii calcRadiiFor(const BorderData& border, LayoutSize size)
{
    return FloatRoundedRect::Radii(
        IntSize(valueForLength(border.topLeft().width(), size.width()),
            valueForLength(border.topLeft().height(), size.height())),
        IntSize(valueForLength(border.topRight().width(), size.width()),
            valueForLength(border.topRight().height(), size.height())),
        IntSize(valueForLength(border.bottomLeft().width(), size.width()),
            valueForLength(border.bottomLeft().height(), size.height())),
        IntSize(valueForLength(border.bottomRight().width(), size.width()),
            valueForLength(border.bottomRight().height(), size.height())));
}

StyleImage* LayoutStyle::listStyleImage() const { return rareInheritedData->listStyleImage.get(); }
void LayoutStyle::setListStyleImage(PassRefPtr<StyleImage> v)
{
    if (rareInheritedData->listStyleImage != v)
        rareInheritedData.access()->listStyleImage = v;
}

Color LayoutStyle::color() const { return inherited->color; }
Color LayoutStyle::visitedLinkColor() const { return inherited->visitedLinkColor; }
void LayoutStyle::setColor(const Color& v) { SET_VAR(inherited, color, v); }
void LayoutStyle::setVisitedLinkColor(const Color& v) { SET_VAR(inherited, visitedLinkColor, v); }

short LayoutStyle::horizontalBorderSpacing() const { return inherited->horizontal_border_spacing; }
short LayoutStyle::verticalBorderSpacing() const { return inherited->vertical_border_spacing; }
void LayoutStyle::setHorizontalBorderSpacing(short v) { SET_VAR(inherited, horizontal_border_spacing, v); }
void LayoutStyle::setVerticalBorderSpacing(short v) { SET_VAR(inherited, vertical_border_spacing, v); }

FloatRoundedRect LayoutStyle::getRoundedBorderFor(const LayoutRect& borderRect, bool includeLogicalLeftEdge, bool includeLogicalRightEdge) const
{
    FloatRoundedRect roundedRect(pixelSnappedIntRect(borderRect));
    if (hasBorderRadius()) {
        FloatRoundedRect::Radii radii = calcRadiiFor(surround->border, borderRect.size());
        roundedRect.includeLogicalEdges(radii, isHorizontalWritingMode(), includeLogicalLeftEdge, includeLogicalRightEdge);
        roundedRect.constrainRadii();
    }
    return roundedRect;
}

FloatRoundedRect LayoutStyle::getRoundedInnerBorderFor(const LayoutRect& borderRect, bool includeLogicalLeftEdge, bool includeLogicalRightEdge) const
{
    bool horizontal = isHorizontalWritingMode();

    int leftWidth = (!horizontal || includeLogicalLeftEdge) ? borderLeftWidth() : 0;
    int rightWidth = (!horizontal || includeLogicalRightEdge) ? borderRightWidth() : 0;
    int topWidth = (horizontal || includeLogicalLeftEdge) ? borderTopWidth() : 0;
    int bottomWidth = (horizontal || includeLogicalRightEdge) ? borderBottomWidth() : 0;

    return getRoundedInnerBorderFor(borderRect, topWidth, bottomWidth, leftWidth, rightWidth, includeLogicalLeftEdge, includeLogicalRightEdge);
}

FloatRoundedRect LayoutStyle::getRoundedInnerBorderFor(const LayoutRect& borderRect,
    int topWidth, int bottomWidth, int leftWidth, int rightWidth, bool includeLogicalLeftEdge, bool includeLogicalRightEdge) const
{
    LayoutRect innerRect(borderRect.x() + leftWidth,
        borderRect.y() + topWidth,
        borderRect.width() - leftWidth - rightWidth,
        borderRect.height() - topWidth - bottomWidth);

    FloatRoundedRect roundedRect(pixelSnappedIntRect(innerRect));

    if (hasBorderRadius()) {
        FloatRoundedRect::Radii radii = getRoundedBorderFor(borderRect).radii();
        radii.shrink(topWidth, bottomWidth, leftWidth, rightWidth);
        roundedRect.includeLogicalEdges(radii, isHorizontalWritingMode(), includeLogicalLeftEdge, includeLogicalRightEdge);
    }
    return roundedRect;
}

static bool allLayersAreFixed(const FillLayer& layer)
{
    for (const FillLayer* currLayer = &layer; currLayer; currLayer = currLayer->next()) {
        if (!currLayer->image() || currLayer->attachment() != FixedBackgroundAttachment)
            return false;
    }

    return true;
}

bool LayoutStyle::hasEntirelyFixedBackground() const
{
    return allLayersAreFixed(backgroundLayers());
}

const CounterDirectiveMap* LayoutStyle::counterDirectives() const
{
    return rareNonInheritedData->m_counterDirectives.get();
}

CounterDirectiveMap& LayoutStyle::accessCounterDirectives()
{
    OwnPtr<CounterDirectiveMap>& map = rareNonInheritedData.access()->m_counterDirectives;
    if (!map)
        map = adoptPtr(new CounterDirectiveMap);
    return *map;
}

const CounterDirectives LayoutStyle::getCounterDirectives(const AtomicString& identifier) const
{
    if (const CounterDirectiveMap* directives = counterDirectives())
        return directives->get(identifier);
    return CounterDirectives();
}

void LayoutStyle::clearIncrementDirectives()
{
    if (!counterDirectives())
        return;

    // This makes us copy even if we may not be removing any items.
    CounterDirectiveMap& map = accessCounterDirectives();
    typedef CounterDirectiveMap::iterator Iterator;

    Iterator end = map.end();
    for (Iterator it = map.begin(); it != end; ++it)
        it->value.clearIncrement();
}

void LayoutStyle::clearResetDirectives()
{
    if (!counterDirectives())
        return;

    // This makes us copy even if we may not be removing any items.
    CounterDirectiveMap& map = accessCounterDirectives();
    typedef CounterDirectiveMap::iterator Iterator;

    Iterator end = map.end();
    for (Iterator it = map.begin(); it != end; ++it)
        it->value.clearReset();
}

const AtomicString& LayoutStyle::hyphenString() const
{
    const AtomicString& hyphenationString = rareInheritedData.get()->hyphenationString;
    if (!hyphenationString.isNull())
        return hyphenationString;

    // FIXME: This should depend on locale.
    DEFINE_STATIC_LOCAL(AtomicString, hyphenMinusString, (&hyphenMinus, 1));
    DEFINE_STATIC_LOCAL(AtomicString, hyphenString, (&hyphen, 1));
    return font().primaryFontHasGlyphForCharacter(hyphen) ? hyphenString : hyphenMinusString;
}

const AtomicString& LayoutStyle::textEmphasisMarkString() const
{
    switch (textEmphasisMark()) {
    case TextEmphasisMarkNone:
        return nullAtom;
    case TextEmphasisMarkCustom:
        return textEmphasisCustomMark();
    case TextEmphasisMarkDot: {
        DEFINE_STATIC_LOCAL(AtomicString, filledDotString, (&bullet, 1));
        DEFINE_STATIC_LOCAL(AtomicString, openDotString, (&whiteBullet, 1));
        return textEmphasisFill() == TextEmphasisFillFilled ? filledDotString : openDotString;
    }
    case TextEmphasisMarkCircle: {
        DEFINE_STATIC_LOCAL(AtomicString, filledCircleString, (&blackCircle, 1));
        DEFINE_STATIC_LOCAL(AtomicString, openCircleString, (&whiteCircle, 1));
        return textEmphasisFill() == TextEmphasisFillFilled ? filledCircleString : openCircleString;
    }
    case TextEmphasisMarkDoubleCircle: {
        DEFINE_STATIC_LOCAL(AtomicString, filledDoubleCircleString, (&fisheye, 1));
        DEFINE_STATIC_LOCAL(AtomicString, openDoubleCircleString, (&bullseye, 1));
        return textEmphasisFill() == TextEmphasisFillFilled ? filledDoubleCircleString : openDoubleCircleString;
    }
    case TextEmphasisMarkTriangle: {
        DEFINE_STATIC_LOCAL(AtomicString, filledTriangleString, (&blackUpPointingTriangle, 1));
        DEFINE_STATIC_LOCAL(AtomicString, openTriangleString, (&whiteUpPointingTriangle, 1));
        return textEmphasisFill() == TextEmphasisFillFilled ? filledTriangleString : openTriangleString;
    }
    case TextEmphasisMarkSesame: {
        DEFINE_STATIC_LOCAL(AtomicString, filledSesameString, (&sesameDot, 1));
        DEFINE_STATIC_LOCAL(AtomicString, openSesameString, (&whiteSesameDot, 1));
        return textEmphasisFill() == TextEmphasisFillFilled ? filledSesameString : openSesameString;
    }
    case TextEmphasisMarkAuto:
        ASSERT_NOT_REACHED();
        return nullAtom;
    }

    ASSERT_NOT_REACHED();
    return nullAtom;
}

CSSAnimationData& LayoutStyle::accessAnimations()
{
    if (!rareNonInheritedData.access()->m_animations)
        rareNonInheritedData.access()->m_animations = CSSAnimationData::create();
    return *rareNonInheritedData->m_animations;
}

CSSTransitionData& LayoutStyle::accessTransitions()
{
    if (!rareNonInheritedData.access()->m_transitions)
        rareNonInheritedData.access()->m_transitions = CSSTransitionData::create();
    return *rareNonInheritedData->m_transitions;
}

const Font& LayoutStyle::font() const { return inherited->font; }
const FontMetrics& LayoutStyle::fontMetrics() const { return inherited->font.fontMetrics(); }
const FontDescription& LayoutStyle::fontDescription() const { return inherited->font.fontDescription(); }
float LayoutStyle::specifiedFontSize() const { return fontDescription().specifiedSize(); }
float LayoutStyle::computedFontSize() const { return fontDescription().computedSize(); }
int LayoutStyle::fontSize() const { return fontDescription().computedPixelSize(); }
FontWeight LayoutStyle::fontWeight() const { return fontDescription().weight(); }
FontStretch LayoutStyle::fontStretch() const { return fontDescription().stretch(); }

TextDecoration LayoutStyle::textDecorationsInEffect() const
{
    int decorations = 0;

    const Vector<AppliedTextDecoration>& applied = appliedTextDecorations();

    for (size_t i = 0; i < applied.size(); ++i)
        decorations |= applied[i].line();

    return static_cast<TextDecoration>(decorations);
}

const Vector<AppliedTextDecoration>& LayoutStyle::appliedTextDecorations() const
{
    if (!inherited_flags.m_textUnderline && !rareInheritedData->appliedTextDecorations) {
        DEFINE_STATIC_LOCAL(Vector<AppliedTextDecoration>, empty, ());
        return empty;
    }
    if (inherited_flags.m_textUnderline) {
        DEFINE_STATIC_LOCAL(Vector<AppliedTextDecoration>, underline, (1, AppliedTextDecoration(TextDecorationUnderline)));
        return underline;
    }

    return rareInheritedData->appliedTextDecorations->vector();
}

float LayoutStyle::wordSpacing() const { return fontDescription().wordSpacing(); }
float LayoutStyle::letterSpacing() const { return fontDescription().letterSpacing(); }

bool LayoutStyle::setFontDescription(const FontDescription& v)
{
    if (inherited->font.fontDescription() != v) {
        inherited.access()->font = Font(v);
        return true;
    }
    return false;
}

const Length& LayoutStyle::specifiedLineHeight() const { return inherited->line_height; }
Length LayoutStyle::lineHeight() const
{
    const Length& lh = inherited->line_height;
    // Unlike fontDescription().computedSize() and hence fontSize(), this is
    // recalculated on demand as we only store the specified line height.
    // FIXME: Should consider scaling the fixed part of any calc expressions
    // too, though this involves messily poking into CalcExpressionLength.
    float multiplier = textAutosizingMultiplier();
    if (multiplier > 1 && lh.isFixed())
        return Length(TextAutosizer::computeAutosizedFontSize(lh.value(), multiplier), Fixed);

    return lh;
}

void LayoutStyle::setLineHeight(const Length& specifiedLineHeight) { SET_VAR(inherited, line_height, specifiedLineHeight); }

int LayoutStyle::computedLineHeight() const
{
    const Length& lh = lineHeight();

    // Negative value means the line height is not set. Use the font's built-in spacing.
    if (lh.isNegative())
        return fontMetrics().lineSpacing();

    if (lh.isPercent())
        return minimumValueForLength(lh, fontSize());

    return lh.value();
}

void LayoutStyle::setWordSpacing(float wordSpacing)
{
    FontSelector* currentFontSelector = font().fontSelector();
    FontDescription desc(fontDescription());
    desc.setWordSpacing(wordSpacing);
    setFontDescription(desc);
    font().update(currentFontSelector);
}

void LayoutStyle::setLetterSpacing(float letterSpacing)
{
    FontSelector* currentFontSelector = font().fontSelector();
    FontDescription desc(fontDescription());
    desc.setLetterSpacing(letterSpacing);
    setFontDescription(desc);
    font().update(currentFontSelector);
}

void LayoutStyle::setTextAutosizingMultiplier(float multiplier)
{
    SET_VAR(inherited, textAutosizingMultiplier, multiplier);

    float size = specifiedFontSize();

    ASSERT(std::isfinite(size));
    if (!std::isfinite(size) || size < 0)
        size = 0;
    else
        size = std::min(maximumAllowedFontSize, size);

    FontSelector* currentFontSelector = font().fontSelector();
    FontDescription desc(fontDescription());
    desc.setSpecifiedSize(size);
    desc.setComputedSize(size);

    if (multiplier > 1) {
        float autosizedFontSize = TextAutosizer::computeAutosizedFontSize(size, multiplier);
        desc.setComputedSize(std::min(maximumAllowedFontSize, autosizedFontSize));
    }

    setFontDescription(desc);
    font().update(currentFontSelector);
}

void LayoutStyle::addAppliedTextDecoration(const AppliedTextDecoration& decoration)
{
    RefPtr<AppliedTextDecorationList>& list = rareInheritedData.access()->appliedTextDecorations;

    if (!list)
        list = AppliedTextDecorationList::create();
    else if (!list->hasOneRef())
        list = list->copy();

    if (inherited_flags.m_textUnderline) {
        inherited_flags.m_textUnderline = false;
        list->append(AppliedTextDecoration(TextDecorationUnderline));
    }

    list->append(decoration);
}

void LayoutStyle::applyTextDecorations()
{
    if (textDecoration() == TextDecorationNone)
        return;

    TextDecorationStyle style = textDecorationStyle();
    StyleColor styleColor = decorationColorIncludingFallback(insideLink() == InsideVisitedLink);

    int decorations = textDecoration();

    if (decorations & TextDecorationUnderline) {
        // To save memory, we don't use AppliedTextDecoration objects in the
        // common case of a single simple underline.
        AppliedTextDecoration underline(TextDecorationUnderline, style, styleColor);

        if (!rareInheritedData->appliedTextDecorations && underline.isSimpleUnderline())
            inherited_flags.m_textUnderline = true;
        else
            addAppliedTextDecoration(underline);
    }
    if (decorations & TextDecorationOverline)
        addAppliedTextDecoration(AppliedTextDecoration(TextDecorationOverline, style, styleColor));
    if (decorations & TextDecorationLineThrough)
        addAppliedTextDecoration(AppliedTextDecoration(TextDecorationLineThrough, style, styleColor));
}

void LayoutStyle::clearAppliedTextDecorations()
{
    inherited_flags.m_textUnderline = false;

    if (rareInheritedData->appliedTextDecorations)
        rareInheritedData.access()->appliedTextDecorations = nullptr;
}

void LayoutStyle::clearMultiCol()
{
    rareNonInheritedData.access()->m_multiCol = nullptr;
    rareNonInheritedData.access()->m_multiCol.init();
}

StyleColor LayoutStyle::decorationColorIncludingFallback(bool visitedLink) const
{
    StyleColor styleColor = visitedLink ? visitedLinkTextDecorationColor() : textDecorationColor();

    if (!styleColor.isCurrentColor())
        return styleColor;

    if (textStrokeWidth()) {
        // Prefer stroke color if possible, but not if it's fully transparent.
        StyleColor textStrokeStyleColor = visitedLink ? visitedLinkTextStrokeColor() : textStrokeColor();
        if (!textStrokeStyleColor.isCurrentColor() && textStrokeStyleColor.color().alpha())
            return textStrokeStyleColor;
    }

    return visitedLink ? visitedLinkTextFillColor() : textFillColor();
}

Color LayoutStyle::colorIncludingFallback(int colorProperty, bool visitedLink) const
{
    StyleColor result(StyleColor::currentColor());
    EBorderStyle borderStyle = BNONE;
    switch (colorProperty) {
    case CSSPropertyBackgroundColor:
        result = visitedLink ? visitedLinkBackgroundColor() : backgroundColor();
        break;
    case CSSPropertyBorderLeftColor:
        result = visitedLink ? visitedLinkBorderLeftColor() : borderLeftColor();
        borderStyle = borderLeftStyle();
        break;
    case CSSPropertyBorderRightColor:
        result = visitedLink ? visitedLinkBorderRightColor() : borderRightColor();
        borderStyle = borderRightStyle();
        break;
    case CSSPropertyBorderTopColor:
        result = visitedLink ? visitedLinkBorderTopColor() : borderTopColor();
        borderStyle = borderTopStyle();
        break;
    case CSSPropertyBorderBottomColor:
        result = visitedLink ? visitedLinkBorderBottomColor() : borderBottomColor();
        borderStyle = borderBottomStyle();
        break;
    case CSSPropertyColor:
        result = visitedLink ? visitedLinkColor() : color();
        break;
    case CSSPropertyOutlineColor:
        result = visitedLink ? visitedLinkOutlineColor() : outlineColor();
        break;
    case CSSPropertyWebkitColumnRuleColor:
        result = visitedLink ? visitedLinkColumnRuleColor() : columnRuleColor();
        break;
    case CSSPropertyWebkitTextEmphasisColor:
        result = visitedLink ? visitedLinkTextEmphasisColor() : textEmphasisColor();
        break;
    case CSSPropertyWebkitTextFillColor:
        result = visitedLink ? visitedLinkTextFillColor() : textFillColor();
        break;
    case CSSPropertyWebkitTextStrokeColor:
        result = visitedLink ? visitedLinkTextStrokeColor() : textStrokeColor();
        break;
    case CSSPropertyFloodColor:
        result = floodColor();
        break;
    case CSSPropertyLightingColor:
        result = lightingColor();
        break;
    case CSSPropertyStopColor:
        result = stopColor();
        break;
    case CSSPropertyWebkitTapHighlightColor:
        result = tapHighlightColor();
        break;
    case CSSPropertyTextDecorationColor:
        result = decorationColorIncludingFallback(visitedLink);
        break;
    default:
        ASSERT_NOT_REACHED();
        break;
    }

    if (!result.isCurrentColor())
        return result.color();

    // FIXME: Treating styled borders with initial color differently causes problems
    // See crbug.com/316559, crbug.com/276231
    if (!visitedLink && (borderStyle == INSET || borderStyle == OUTSET || borderStyle == RIDGE || borderStyle == GROOVE))
        return Color(238, 238, 238);
    return visitedLink ? visitedLinkColor() : color();
}

Color LayoutStyle::visitedDependentColor(int colorProperty) const
{
    Color unvisitedColor = colorIncludingFallback(colorProperty, false);
    if (insideLink() != InsideVisitedLink)
        return unvisitedColor;

    Color visitedColor = colorIncludingFallback(colorProperty, true);

    // FIXME: Technically someone could explicitly specify the color transparent, but for now we'll just
    // assume that if the background color is transparent that it wasn't set. Note that it's weird that
    // we're returning unvisited info for a visited link, but given our restriction that the alpha values
    // have to match, it makes more sense to return the unvisited background color if specified than it
    // does to return black. This behavior matches what Firefox 4 does as well.
    if (colorProperty == CSSPropertyBackgroundColor && visitedColor == Color::transparent)
        return unvisitedColor;

    // Take the alpha from the unvisited color, but get the RGB values from the visited color.
    return Color(visitedColor.red(), visitedColor.green(), visitedColor.blue(), unvisitedColor.alpha());
}

const BorderValue& LayoutStyle::borderBefore() const
{
    switch (writingMode()) {
    case TopToBottomWritingMode:
        return borderTop();
    case BottomToTopWritingMode:
        return borderBottom();
    case LeftToRightWritingMode:
        return borderLeft();
    case RightToLeftWritingMode:
        return borderRight();
    }
    ASSERT_NOT_REACHED();
    return borderTop();
}

const BorderValue& LayoutStyle::borderAfter() const
{
    switch (writingMode()) {
    case TopToBottomWritingMode:
        return borderBottom();
    case BottomToTopWritingMode:
        return borderTop();
    case LeftToRightWritingMode:
        return borderRight();
    case RightToLeftWritingMode:
        return borderLeft();
    }
    ASSERT_NOT_REACHED();
    return borderBottom();
}

const BorderValue& LayoutStyle::borderStart() const
{
    if (isHorizontalWritingMode())
        return isLeftToRightDirection() ? borderLeft() : borderRight();
    return isLeftToRightDirection() ? borderTop() : borderBottom();
}

const BorderValue& LayoutStyle::borderEnd() const
{
    if (isHorizontalWritingMode())
        return isLeftToRightDirection() ? borderRight() : borderLeft();
    return isLeftToRightDirection() ? borderBottom() : borderTop();
}

unsigned short LayoutStyle::borderBeforeWidth() const
{
    switch (writingMode()) {
    case TopToBottomWritingMode:
        return borderTopWidth();
    case BottomToTopWritingMode:
        return borderBottomWidth();
    case LeftToRightWritingMode:
        return borderLeftWidth();
    case RightToLeftWritingMode:
        return borderRightWidth();
    }
    ASSERT_NOT_REACHED();
    return borderTopWidth();
}

unsigned short LayoutStyle::borderAfterWidth() const
{
    switch (writingMode()) {
    case TopToBottomWritingMode:
        return borderBottomWidth();
    case BottomToTopWritingMode:
        return borderTopWidth();
    case LeftToRightWritingMode:
        return borderRightWidth();
    case RightToLeftWritingMode:
        return borderLeftWidth();
    }
    ASSERT_NOT_REACHED();
    return borderBottomWidth();
}

unsigned short LayoutStyle::borderStartWidth() const
{
    if (isHorizontalWritingMode())
        return isLeftToRightDirection() ? borderLeftWidth() : borderRightWidth();
    return isLeftToRightDirection() ? borderTopWidth() : borderBottomWidth();
}

unsigned short LayoutStyle::borderEndWidth() const
{
    if (isHorizontalWritingMode())
        return isLeftToRightDirection() ? borderRightWidth() : borderLeftWidth();
    return isLeftToRightDirection() ? borderBottomWidth() : borderTopWidth();
}

void LayoutStyle::setMarginStart(const Length& margin)
{
    if (isHorizontalWritingMode()) {
        if (isLeftToRightDirection())
            setMarginLeft(margin);
        else
            setMarginRight(margin);
    } else {
        if (isLeftToRightDirection())
            setMarginTop(margin);
        else
            setMarginBottom(margin);
    }
}

void LayoutStyle::setMarginEnd(const Length& margin)
{
    if (isHorizontalWritingMode()) {
        if (isLeftToRightDirection())
            setMarginRight(margin);
        else
            setMarginLeft(margin);
    } else {
        if (isLeftToRightDirection())
            setMarginBottom(margin);
        else
            setMarginTop(margin);
    }
}

void LayoutStyle::setMotionPath(PassRefPtr<StyleMotionPath> path)
{
    ASSERT(path);
    rareNonInheritedData.access()->m_transform.access()->m_motion.m_path = path;
}

void LayoutStyle::resetMotionPath()
{
    rareNonInheritedData.access()->m_transform.access()->m_motion.m_path = nullptr;
}

bool LayoutStyle::columnRuleEquivalent(const LayoutStyle* otherStyle) const
{
    return columnRuleStyle() == otherStyle->columnRuleStyle()
        && columnRuleWidth() == otherStyle->columnRuleWidth()
        && visitedDependentColor(CSSPropertyWebkitColumnRuleColor) == otherStyle->visitedDependentColor(CSSPropertyWebkitColumnRuleColor);
}

TextEmphasisMark LayoutStyle::textEmphasisMark() const
{
    TextEmphasisMark mark = static_cast<TextEmphasisMark>(rareInheritedData->textEmphasisMark);
    if (mark != TextEmphasisMarkAuto)
        return mark;

    if (isHorizontalWritingMode())
        return TextEmphasisMarkDot;

    return TextEmphasisMarkSesame;
}

Color LayoutStyle::initialTapHighlightColor()
{
    return LayoutTheme::tapHighlightColor();
}

#if ENABLE(OILPAN)
const FilterOperations& LayoutStyle::initialFilter()
{
    DEFINE_STATIC_LOCAL(Persistent<FilterOperationsWrapper>, ops, (FilterOperationsWrapper::create()));
    return ops->operations();
}
#endif

LayoutRectOutsets LayoutStyle::imageOutsets(const NinePieceImage& image) const
{
    return LayoutRectOutsets(
        NinePieceImage::computeOutset(image.outset().top(), borderTopWidth()),
        NinePieceImage::computeOutset(image.outset().right(), borderRightWidth()),
        NinePieceImage::computeOutset(image.outset().bottom(), borderBottomWidth()),
        NinePieceImage::computeOutset(image.outset().left(), borderLeftWidth()));
}

void LayoutStyle::setBorderImageSource(PassRefPtr<StyleImage> image)
{
    if (surround->border.m_image.image() == image.get())
        return;
    surround.access()->border.m_image.setImage(image);
}

void LayoutStyle::setBorderImageSlices(const LengthBox& slices)
{
    if (surround->border.m_image.imageSlices() == slices)
        return;
    surround.access()->border.m_image.setImageSlices(slices);
}

void LayoutStyle::setBorderImageWidth(const BorderImageLengthBox& slices)
{
    if (surround->border.m_image.borderSlices() == slices)
        return;
    surround.access()->border.m_image.setBorderSlices(slices);
}

void LayoutStyle::setBorderImageOutset(const BorderImageLengthBox& outset)
{
    if (surround->border.m_image.outset() == outset)
        return;
    surround.access()->border.m_image.setOutset(outset);
}

bool LayoutStyle::borderObscuresBackground() const
{
    if (!hasBorder())
        return false;

    // Bail if we have any border-image for now. We could look at the image alpha to improve this.
    if (borderImage().image())
        return false;

    BorderEdge edges[4];
    getBorderEdgeInfo(edges);

    for (int i = BSTop; i <= BSLeft; ++i) {
        const BorderEdge& currEdge = edges[i];
        if (!currEdge.obscuresBackground())
            return false;
    }

    return true;
}

void LayoutStyle::getBorderEdgeInfo(BorderEdge edges[], bool includeLogicalLeftEdge, bool includeLogicalRightEdge) const
{
    bool horizontal = isHorizontalWritingMode();

    edges[BSTop] = BorderEdge(borderTopWidth(),
        visitedDependentColor(CSSPropertyBorderTopColor),
        borderTopStyle(),
        borderTopIsTransparent(),
        horizontal || includeLogicalLeftEdge);

    edges[BSRight] = BorderEdge(borderRightWidth(),
        visitedDependentColor(CSSPropertyBorderRightColor),
        borderRightStyle(),
        borderRightIsTransparent(),
        !horizontal || includeLogicalRightEdge);

    edges[BSBottom] = BorderEdge(borderBottomWidth(),
        visitedDependentColor(CSSPropertyBorderBottomColor),
        borderBottomStyle(),
        borderBottomIsTransparent(),
        horizontal || includeLogicalRightEdge);

    edges[BSLeft] = BorderEdge(borderLeftWidth(),
        visitedDependentColor(CSSPropertyBorderLeftColor),
        borderLeftStyle(),
        borderLeftIsTransparent(),
        !horizontal || includeLogicalLeftEdge);
}

} // namespace blink