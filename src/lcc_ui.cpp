/*
 *  Copyright © 2017-2025 Wellington Wallace
 *  Localization Cue Correction plugin developed by Antti S. Lankila <alankila@bel.fi>
 *
 *  This file is part of Easy Effects.
 *
 *  Easy Effects is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Easy Effects is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Easy Effects. If not, see <https://www.gnu.org/licenses/>.
 */

#include "lcc_ui.hpp"
#include <STTypes.h>
#include <gio/gio.h>
#include <glib-object.h>
#include <glib.h>
#include <glibconfig.h>
#include <gobject/gobject.h>
#include <gtk/gtk.h>
#include <sigc++/connection.h>
#include <memory>
#include <string>
#include <vector>
#include "lcc.hpp"
#include "tags_resources.hpp"
#include "tags_schema.hpp"
#include "ui_helpers.hpp"
#include "util.hpp"

namespace ui::lcc_box {

struct Data {
 public:
  ~Data() { util::debug("data struct destroyed"); }

  uint serial = 0U;

  std::shared_ptr<LCC> lcc;

  std::vector<sigc::connection> connections;

  std::vector<gulong> gconnections;
};

struct _LCCBox {
  GtkBox parent_instance;

  GtkScale *input_gain, *output_gain;

  GtkLevelBar *input_level_left, *input_level_right, *output_level_left, *output_level_right;

  GtkLabel *input_level_left_label, *input_level_right_label, *output_level_left_label, *output_level_right_label,
      *plugin_credit;

  GtkToggleButton *phantom_center_only;

  GtkSpinButton *delay_us, *decay_db;

  GSettings* settings;

  Data* data;
};

// NOLINTNEXTLINE
G_DEFINE_TYPE(LCCBox, lcc_box, GTK_TYPE_BOX)

void on_reset(LCCBox* self, GtkButton* btn) {
  util::reset_all_keys_except(self->settings);
}

void setup(LCCBox* self, std::shared_ptr<LCC> lcc, const std::string& schema_path) {
  auto serial = get_new_filter_serial();

  self->data->serial = serial;

  g_object_set_data(G_OBJECT(self), "serial", GUINT_TO_POINTER(serial));

  set_ignore_filter_idle_add(serial, false);

  self->data->lcc = lcc;

  self->settings = g_settings_new_with_path(tags::schema::lcc::id, schema_path.c_str());

  lcc->set_post_messages(true);

  self->data->connections.push_back(lcc->input_level.connect([=](const float left, const float right) {
    g_object_ref(self);

    util::idle_add(
        [=]() {
          if (get_ignore_filter_idle_add(serial)) {
            return;
          }

          update_level(self->input_level_left, self->input_level_left_label, self->input_level_right,
                       self->input_level_right_label, left, right);
        },
        [=]() { g_object_unref(self); });
  }));

  self->data->connections.push_back(lcc->output_level.connect([=](const float left, const float right) {
    g_object_ref(self);

    util::idle_add(
        [=]() {
          if (get_ignore_filter_idle_add(serial)) {
            return;
          }

          update_level(self->output_level_left, self->output_level_left_label, self->output_level_right,
                       self->output_level_right_label, left, right);
        },
        [=]() { g_object_unref(self); });
  }));

  gtk_label_set_text(self->plugin_credit, ui::get_plugin_credit_translated(self->data->lcc->package).c_str());

  gsettings_bind_widgets<"input-gain", "output-gain">(self->settings, self->input_gain, self->output_gain);

  g_settings_bind(self->settings, "phantom-center-only", self->phantom_center_only, "active", G_SETTINGS_BIND_DEFAULT);
  g_settings_bind(self->settings, "delay-us", gtk_spin_button_get_adjustment(self->delay_us), "value", G_SETTINGS_BIND_DEFAULT);
  g_settings_bind(self->settings, "decay-db", gtk_spin_button_get_adjustment(self->decay_db), "value", G_SETTINGS_BIND_DEFAULT);
}

void dispose(GObject* object) {
  auto* self = EE_LCC_BOX(object);

  set_ignore_filter_idle_add(self->data->serial, true);

  for (auto& c : self->data->connections) {
    c.disconnect();
  }

  for (auto& handler_id : self->data->gconnections) {
    g_signal_handler_disconnect(self->settings, handler_id);
  }

  self->data->connections.clear();
  self->data->gconnections.clear();

  g_object_unref(self->settings);

  util::debug("disposed");

  G_OBJECT_CLASS(lcc_box_parent_class)->dispose(object);
}

void finalize(GObject* object) {
  auto* self = EE_LCC_BOX(object);

  delete self->data;

  util::debug("finalized");

  G_OBJECT_CLASS(lcc_box_parent_class)->finalize(object);
}

void lcc_box_class_init(LCCBoxClass* klass) {
  auto* object_class = G_OBJECT_CLASS(klass);
  auto* widget_class = GTK_WIDGET_CLASS(klass);

  object_class->dispose = dispose;
  object_class->finalize = finalize;

  gtk_widget_class_set_template_from_resource(widget_class, tags::resources::lcc_ui);

  gtk_widget_class_bind_template_child(widget_class, LCCBox, input_gain);
  gtk_widget_class_bind_template_child(widget_class, LCCBox, output_gain);
  gtk_widget_class_bind_template_child(widget_class, LCCBox, input_level_left);
  gtk_widget_class_bind_template_child(widget_class, LCCBox, input_level_right);
  gtk_widget_class_bind_template_child(widget_class, LCCBox, output_level_left);
  gtk_widget_class_bind_template_child(widget_class, LCCBox, output_level_right);
  gtk_widget_class_bind_template_child(widget_class, LCCBox, input_level_left_label);
  gtk_widget_class_bind_template_child(widget_class, LCCBox, input_level_right_label);
  gtk_widget_class_bind_template_child(widget_class, LCCBox, output_level_left_label);
  gtk_widget_class_bind_template_child(widget_class, LCCBox, output_level_right_label);
  gtk_widget_class_bind_template_child(widget_class, LCCBox, plugin_credit);

  gtk_widget_class_bind_template_child(widget_class, LCCBox, phantom_center_only);
  gtk_widget_class_bind_template_child(widget_class, LCCBox, delay_us);
  gtk_widget_class_bind_template_child(widget_class, LCCBox, decay_db);

  gtk_widget_class_bind_template_callback(widget_class, on_reset);
}

void lcc_box_init(LCCBox* self) {
  gtk_widget_init_template(GTK_WIDGET(self));

  self->data = new Data();

  prepare_spinbuttons<"us">(self->delay_us);
  prepare_spinbuttons<"dB">(self->decay_db);

  prepare_scales<"dB">(self->input_gain, self->output_gain);
}

auto create() -> LCCBox* {
  return static_cast<LCCBox*>(g_object_new(EE_TYPE_LCC_BOX, nullptr));
}

}  // namespace ui::lcc_box
