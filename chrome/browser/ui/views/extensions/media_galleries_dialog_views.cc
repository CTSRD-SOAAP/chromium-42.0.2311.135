// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/media_galleries_dialog_views.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/views/extensions/media_gallery_checkbox_view.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/locale_settings.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/web_modal/popup_manager.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_client_view.h"

namespace {

const int kScrollAreaHeight = 192;

// This container has the right Layout() impl to use within a ScrollView.
class ScrollableView : public views::View {
 public:
  ScrollableView() {}
  ~ScrollableView() override {}

  void Layout() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ScrollableView);
};

void ScrollableView::Layout() {
  gfx::Size pref = GetPreferredSize();
  int width = pref.width();
  int height = pref.height();
  if (parent()) {
    width = parent()->width();
    height = std::max(parent()->height(), height);
  }
  SetBounds(x(), y(), width, height);

  views::View::Layout();
}

}  // namespace

MediaGalleriesDialogViews::MediaGalleriesDialogViews(
    MediaGalleriesDialogController* controller)
    : controller_(controller),
      contents_(new views::View()),
      auxiliary_button_(NULL),
      confirm_available_(false),
      accepted_(false) {
  InitChildViews();
  if (ControllerHasWebContents()) {
    constrained_window::ShowWebModalDialogViews(this,
                                                controller->WebContents());
  }
}

MediaGalleriesDialogViews::~MediaGalleriesDialogViews() {
  if (!ControllerHasWebContents())
    delete contents_;
}

void MediaGalleriesDialogViews::AcceptDialogForTesting() {
  accepted_ = true;

  web_modal::PopupManager* popup_manager =
      web_modal::PopupManager::FromWebContents(controller_->WebContents());
  DCHECK(popup_manager);
  popup_manager->CloseAllDialogsForTesting(controller_->WebContents());
}

void MediaGalleriesDialogViews::InitChildViews() {
  // Outer dialog layout.
  contents_->RemoveAllChildViews(true);
  checkbox_map_.clear();

  int dialog_content_width = views::Widget::GetLocalizedContentsWidth(
      IDS_MEDIA_GALLERIES_DIALOG_CONTENT_WIDTH_CHARS);
  views::GridLayout* layout = views::GridLayout::CreatePanel(contents_);
  contents_->SetLayoutManager(layout);

  int column_set_id = 0;
  views::ColumnSet* columns = layout->AddColumnSet(column_set_id);
  columns->AddColumn(views::GridLayout::LEADING,
                     views::GridLayout::LEADING,
                     1,
                     views::GridLayout::FIXED,
                     dialog_content_width,
                     0);

  // Message text.
  views::Label* subtext = new views::Label(controller_->GetSubtext());
  subtext->SetMultiLine(true);
  subtext->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  layout->StartRow(0, column_set_id);
  layout->AddView(
      subtext, 1, 1,
      views::GridLayout::FILL, views::GridLayout::LEADING,
      dialog_content_width, subtext->GetHeightForWidth(dialog_content_width));
  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

  // Scrollable area for checkboxes.
  ScrollableView* scroll_container = new ScrollableView();
  scroll_container->SetLayoutManager(new views::BoxLayout(
      views::BoxLayout::kVertical, 0, 0,
      views::kRelatedControlSmallVerticalSpacing));
  scroll_container->SetBorder(
      views::Border::CreateEmptyBorder(views::kRelatedControlVerticalSpacing,
                                       0,
                                       views::kRelatedControlVerticalSpacing,
                                       0));

  std::vector<base::string16> section_headers =
      controller_->GetSectionHeaders();
  for (size_t i = 0; i < section_headers.size(); i++) {
    MediaGalleriesDialogController::Entries entries =
        controller_->GetSectionEntries(i);

    // Header and separator line.
    if (!section_headers[i].empty() && !entries.empty()) {
      views::Separator* separator = new views::Separator(
          views::Separator::HORIZONTAL);
      scroll_container->AddChildView(separator);

      views::Label* header = new views::Label(section_headers[i]);
      header->SetMultiLine(true);
      header->SetHorizontalAlignment(gfx::ALIGN_LEFT);
      header->SetBorder(views::Border::CreateEmptyBorder(
          views::kRelatedControlVerticalSpacing,
          views::kPanelHorizMargin,
          views::kRelatedControlVerticalSpacing,
          0));
      scroll_container->AddChildView(header);
    }

    // Checkboxes.
    MediaGalleriesDialogController::Entries::const_iterator iter;
    for (iter = entries.begin(); iter != entries.end(); ++iter) {
      int spacing = 0;
      if (iter + 1 == entries.end())
        spacing = views::kRelatedControlSmallVerticalSpacing;
      AddOrUpdateGallery(*iter, scroll_container, spacing);
    }
  }

  confirm_available_ = controller_->IsAcceptAllowed();

  // Add the scrollable area to the outer dialog view. It will squeeze against
  // the title/subtitle and buttons to occupy all available space in the dialog.
  views::ScrollView* scroll_view =
      views::ScrollView::CreateScrollViewWithBorder();
  scroll_view->SetContents(scroll_container);
  layout->StartRowWithPadding(1, column_set_id,
                              0, views::kRelatedControlVerticalSpacing);
  layout->AddView(scroll_view, 1, 1,
                  views::GridLayout::FILL, views::GridLayout::FILL,
                  dialog_content_width, kScrollAreaHeight);
}

void MediaGalleriesDialogViews::UpdateGalleries() {
  InitChildViews();
  contents_->Layout();

  if (ControllerHasWebContents())
    GetWidget()->client_view()->AsDialogClientView()->UpdateDialogButtons();
}

bool MediaGalleriesDialogViews::AddOrUpdateGallery(
    const MediaGalleriesDialogController::Entry& gallery,
    views::View* container,
    int trailing_vertical_space) {
  bool show_folder_viewer = controller_->ShouldShowFolderViewer(gallery);

  CheckboxMap::iterator iter = checkbox_map_.find(gallery.pref_info.pref_id);
  if (iter != checkbox_map_.end()) {
    views::Checkbox* checkbox = iter->second->checkbox();
    checkbox->SetChecked(gallery.selected);
    checkbox->SetText(gallery.pref_info.GetGalleryDisplayName());
    checkbox->SetTooltipText(gallery.pref_info.GetGalleryTooltip());
    base::string16 details = gallery.pref_info.GetGalleryAdditionalDetails();
    iter->second->secondary_text()->SetText(details);
    iter->second->secondary_text()->SetVisible(details.length() > 0);
    iter->second->folder_viewer_button()->SetVisible(show_folder_viewer);
    return false;
  }

  MediaGalleryCheckboxView* gallery_view =
      new MediaGalleryCheckboxView(gallery.pref_info, show_folder_viewer,
                                   trailing_vertical_space, this, this);
  gallery_view->checkbox()->SetChecked(gallery.selected);
  container->AddChildView(gallery_view);
  checkbox_map_[gallery.pref_info.pref_id] = gallery_view;

  return true;
}

base::string16 MediaGalleriesDialogViews::GetWindowTitle() const {
  return controller_->GetHeader();
}

void MediaGalleriesDialogViews::DeleteDelegate() {
  controller_->DialogFinished(accepted_);
}

views::Widget* MediaGalleriesDialogViews::GetWidget() {
  return contents_->GetWidget();
}

const views::Widget* MediaGalleriesDialogViews::GetWidget() const {
  return contents_->GetWidget();
}

views::View* MediaGalleriesDialogViews::GetContentsView() {
  return contents_;
}

base::string16 MediaGalleriesDialogViews::GetDialogButtonLabel(
    ui::DialogButton button) const {
  if (button == ui::DIALOG_BUTTON_OK)
    return controller_->GetAcceptButtonText();
  return l10n_util::GetStringUTF16(IDS_MEDIA_GALLERIES_DIALOG_CANCEL);
}

bool MediaGalleriesDialogViews::IsDialogButtonEnabled(
    ui::DialogButton button) const {
  return button != ui::DIALOG_BUTTON_OK || confirm_available_;
}

ui::ModalType MediaGalleriesDialogViews::GetModalType() const {
  return ui::MODAL_TYPE_CHILD;
}

views::View* MediaGalleriesDialogViews::CreateExtraView() {
  DCHECK(!auxiliary_button_);
  base::string16 button_label = controller_->GetAuxiliaryButtonText();
  if (!button_label.empty()) {
    auxiliary_button_ = new views::LabelButton(this, button_label);
    auxiliary_button_->SetStyle(views::Button::STYLE_BUTTON);
  }
  return auxiliary_button_;
}

bool MediaGalleriesDialogViews::Cancel() {
  return true;
}

bool MediaGalleriesDialogViews::Accept() {
  accepted_ = true;
  return true;
}

void MediaGalleriesDialogViews::ButtonPressed(views::Button* sender,
                                              const ui::Event& /* event */) {
  confirm_available_ = true;

  if (ControllerHasWebContents())
    GetWidget()->client_view()->AsDialogClientView()->UpdateDialogButtons();

  if (sender == auxiliary_button_) {
    controller_->DidClickAuxiliaryButton();
    return;
  }

  for (CheckboxMap::const_iterator iter = checkbox_map_.begin();
       iter != checkbox_map_.end(); ++iter) {
    if (sender == iter->second->checkbox()) {
      controller_->DidToggleEntry(iter->first,
                                  iter->second->checkbox()->checked());
      return;
    }
    if (sender == iter->second->folder_viewer_button()) {
      controller_->DidClickOpenFolderViewer(iter->first);
      return;
    }
  }
}

void MediaGalleriesDialogViews::ShowContextMenuForView(
    views::View* source,
    const gfx::Point& point,
    ui::MenuSourceType source_type) {
  for (CheckboxMap::const_iterator iter = checkbox_map_.begin();
       iter != checkbox_map_.end(); ++iter) {
    if (iter->second->Contains(source)) {
      ShowContextMenu(point, source_type, iter->first);
      return;
    }
  }
}

void MediaGalleriesDialogViews::ShowContextMenu(const gfx::Point& point,
                                                ui::MenuSourceType source_type,
                                                MediaGalleryPrefId id) {
  context_menu_runner_.reset(new views::MenuRunner(
      controller_->GetContextMenu(id),
      views::MenuRunner::HAS_MNEMONICS | views::MenuRunner::CONTEXT_MENU));

  if (context_menu_runner_->RunMenuAt(GetWidget(),
                                      NULL,
                                      gfx::Rect(point.x(), point.y(), 0, 0),
                                      views::MENU_ANCHOR_TOPLEFT,
                                      source_type) ==
      views::MenuRunner::MENU_DELETED) {
    return;
  }
}

bool MediaGalleriesDialogViews::ControllerHasWebContents() const {
  return controller_->WebContents() != NULL;
}

// MediaGalleriesDialogViewsController -----------------------------------------

// static
MediaGalleriesDialog* MediaGalleriesDialog::Create(
    MediaGalleriesDialogController* controller) {
  return new MediaGalleriesDialogViews(controller);
}