/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * GData Client
 * Copyright (C) Philip Withnall 2009–2010 <philip@tecnocode.co.uk>
 *
 * GData Client is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * GData Client is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with GData Client.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * SECTION:gdata-contacts-contact
 * @short_description: GData Contacts contact object
 * @stability: Unstable
 * @include: gdata/services/contacts/gdata-contacts-contact.h
 *
 * #GDataContactsContact is a subclass of #GDataEntry to represent a contact from a Google address book.
 *
 * For more details of Google Contacts' GData API, see the <ulink type="http" url="http://code.google.com/apis/contacts/docs/2.0/reference.html">
 * online documentation</ulink>.
 *
 * Since: 0.2.0
 **/

#include <config.h>
#include <glib.h>
#include <glib/gi18n-lib.h>
#include <libxml/parser.h>
#include <string.h>

#include "gdata-contacts-contact.h"
#include "gdata-parser.h"
#include "gdata-types.h"
#include "gdata-private.h"

/* The maximum number of extended properties the server allows us. See
 * http://code.google.com/apis/contacts/docs/2.0/reference.html#ProjectionsAndExtended.
 * When updating this, make sure to update the API documentation for gdata_contacts_contact_get_extended_property()
 * and gdata_contacts_contact_set_extended_property(). */
#define MAX_N_EXTENDED_PROPERTIES 10

static void gdata_contacts_contact_dispose (GObject *object);
static void gdata_contacts_contact_finalize (GObject *object);
static void gdata_contacts_contact_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec);
static void gdata_contacts_contact_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec);
static void get_xml (GDataParsable *parsable, GString *xml_string);
static gboolean parse_xml (GDataParsable *parsable, xmlDoc *doc, xmlNode *node, gpointer user_data, GError **error);
static void get_namespaces (GDataParsable *parsable, GHashTable *namespaces);
static gchar *get_entry_uri (const gchar *id) G_GNUC_WARN_UNUSED_RESULT;

struct _GDataContactsContactPrivate {
	GTimeVal edited;
	GDataGDName *name;
	GList *email_addresses; /* GDataGDEmailAddress */
	GList *im_addresses; /* GDataGDIMAddress */
	GList *phone_numbers; /* GDataGDPhoneNumber */
	GList *postal_addresses; /* GDataGDPostalAddress */
	GList *organizations; /* GDataGDOrganization */
	GHashTable *extended_properties;
	GHashTable *groups;
	gboolean deleted;
	gchar *photo_etag;
	GList *jots; /* GDataGContactJot */
	gchar *nickname;
	GDate birthday;
	gboolean birthday_has_year; /* contacts can choose to just give the month and day of their birth */
	GList *relations; /* GDataGContactRelation */
	GList *websites; /* GDataGContactWebsite */
	GList *events; /* GDataGContactEvent */
	GList *calendars; /* GDataGContactCalendar */
	gchar *billing_information;
	gchar *directory_server;
	gchar *gender;
	gchar *initials;
	gchar *maiden_name;
	gchar *mileage;
	gchar *occupation;
	gchar *priority;
	gchar *sensitivity;
	gchar *short_name;
	gchar *subject;
};

enum {
	PROP_EDITED = 1,
	PROP_DELETED,
	PROP_HAS_PHOTO,
	PROP_NAME,
	PROP_NICKNAME,
	PROP_BIRTHDAY,
	PROP_BIRTHDAY_HAS_YEAR,
	PROP_BILLING_INFORMATION,
	PROP_DIRECTORY_SERVER,
	PROP_GENDER,
	PROP_INITIALS,
	PROP_MAIDEN_NAME,
	PROP_MILEAGE,
	PROP_OCCUPATION,
	PROP_PRIORITY,
	PROP_SENSITIVITY,
	PROP_SHORT_NAME,
	PROP_SUBJECT
};

G_DEFINE_TYPE (GDataContactsContact, gdata_contacts_contact, GDATA_TYPE_ENTRY)

static void
gdata_contacts_contact_class_init (GDataContactsContactClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	GDataParsableClass *parsable_class = GDATA_PARSABLE_CLASS (klass);
	GDataEntryClass *entry_class = GDATA_ENTRY_CLASS (klass);

	g_type_class_add_private (klass, sizeof (GDataContactsContactPrivate));

	gobject_class->get_property = gdata_contacts_contact_get_property;
	gobject_class->set_property = gdata_contacts_contact_set_property;
	gobject_class->dispose = gdata_contacts_contact_dispose;
	gobject_class->finalize = gdata_contacts_contact_finalize;

	parsable_class->parse_xml = parse_xml;
	parsable_class->get_xml = get_xml;
	parsable_class->get_namespaces = get_namespaces;

	entry_class->get_entry_uri = get_entry_uri;

	/**
	 * GDataContactsContact:edited:
	 *
	 * The last time the contact was edited. If the contact has not been edited yet, the content indicates the time it was created.
	 *
	 * For more information, see the <ulink type="http" url="http://www.atomenabled.org/developers/protocol/#appEdited">
	 * Atom Publishing Protocol specification</ulink>.
	 *
	 * Since: 0.2.0
	 **/
	g_object_class_install_property (gobject_class, PROP_EDITED,
				g_param_spec_boxed ("edited",
					"Edited", "The last time the contact was edited.",
					GDATA_TYPE_G_TIME_VAL,
					G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

	/**
	 * GDataContactsContact:deleted:
	 *
	 * Whether the entry has been deleted.
	 *
	 * Since: 0.2.0
	 **/
	g_object_class_install_property (gobject_class, PROP_DELETED,
				g_param_spec_boolean ("deleted",
					"Deleted", "Whether the entry has been deleted.",
					FALSE,
					G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

	/**
	 * GDataContactsContact:has-photo:
	 *
	 * Whether the contact has a photo.
	 *
	 * Since: 0.4.0
	 **/
	g_object_class_install_property (gobject_class, PROP_HAS_PHOTO,
				g_param_spec_boolean ("has-photo",
					"Has photo?", "Whether the contact has a photo.",
					FALSE,
					G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

	/**
	 * GDataContactsContact:name:
	 *
	 * The contact's name in a structured representation.
	 *
	 * Since: 0.5.0
	 **/
	g_object_class_install_property (gobject_class, PROP_NAME,
				g_param_spec_object ("name",
					"Name", "The contact's name in a structured representation.",
					GDATA_TYPE_GD_NAME,
					G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	/**
	 * GDataContactsContact:nickname:
	 *
	 * The contact's chosen nickname.
	 *
	 * Since: 0.7.0
	 **/
	g_object_class_install_property (gobject_class, PROP_NICKNAME,
				g_param_spec_string ("nickname",
					"Nickname", "The contact's chosen nickname.",
					NULL,
					G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	/**
	 * GDataContactsContact:birthday:
	 *
	 * The contact's birthday.
	 *
	 * Since: 0.7.0
	 **/
	g_object_class_install_property (gobject_class, PROP_BIRTHDAY,
				g_param_spec_boxed ("birthday",
					"Birthday", "The contact's birthday.",
					G_TYPE_DATE,
					G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	/**
	 * GDataContactsContact:birthday-has-year:
	 *
	 * Whether the contact's birthday includes the year of their birth.
	 *
	 * Since: 0.7.0
	 **/
	g_object_class_install_property (gobject_class, PROP_BIRTHDAY_HAS_YEAR,
				g_param_spec_boolean ("birthday-has-year",
					"Birthday has year?", "Whether the contact's birthday includes the year of their birth.",
					FALSE,
					G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	/**
	 * GDataContactsContact:billing-information:
	 *
	 * Billing information for the contact, such as their billing name and address.
	 *
	 * Since: 0.7.0
	 **/
	g_object_class_install_property (gobject_class, PROP_BILLING_INFORMATION,
				g_param_spec_string ("billing-information",
					"Billing information", "Billing information for the contact.",
					NULL,
					G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	/**
	 * GDataContactsContact:directory-server:
	 *
	 * The name or address of a directory server associated with the contact.
	 *
	 * Since: 0.7.0
	 **/
	g_object_class_install_property (gobject_class, PROP_DIRECTORY_SERVER,
				g_param_spec_string ("directory-server",
					"Directory server", "The name or address of a directory server associated with the contact.",
					NULL,
					G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	/**
	 * GDataContactsContact:gender:
	 *
	 * The gender of the contact. For example: %GDATA_CONTACTS_GENDER_MALE or %GDATA_CONTACTS_GENDER_FEMALE.
	 *
	 * Since: 0.7.0
	 **/
	g_object_class_install_property (gobject_class, PROP_GENDER,
				g_param_spec_string ("gender",
					"Gender", "The gender of the contact.",
					NULL,
					G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	/**
	 * GDataContactsContact:initials:
	 *
	 * The initials of the contact.
	 *
	 * Since: 0.7.0
	 **/
	g_object_class_install_property (gobject_class, PROP_INITIALS,
				g_param_spec_string ("initials",
					"Initials", "The initials of the contact.",
					NULL,
					G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	/**
	 * GDataContactsContact:maiden-name:
	 *
	 * The maiden name of the contact.
	 *
	 * Since: 0.7.0
	 **/
	g_object_class_install_property (gobject_class, PROP_MAIDEN_NAME,
				g_param_spec_string ("maiden-name",
					"Maiden name", "The maiden name of the contact.",
					NULL,
					G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	/**
	 * GDataContactsContact:mileage:
	 *
	 * A mileage associated with the contact, such as one for reimbursement purposes. It can be in any format.
	 *
	 * Since: 0.7.0
	 **/
	g_object_class_install_property (gobject_class, PROP_MILEAGE,
				g_param_spec_string ("mileage",
					"Mileage", "A mileage associated with the contact, such as one for reimbursement purposes.",
					NULL,
					G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	/**
	 * GDataContactsContact:occupation:
	 *
	 * The contact's occupation.
	 *
	 * Since: 0.7.0
	 **/
	g_object_class_install_property (gobject_class, PROP_OCCUPATION,
				g_param_spec_string ("occupation",
					"Occupation", "The contact's occupation.",
					NULL,
					G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	/**
	 * GDataContactsContact:priority:
	 *
	 * The contact's importance. For example: %GDATA_CONTACTS_PRIORITY_NORMAL or %GDATA_CONTACTS_PRIORITY_HIGH.
	 *
	 * Since: 0.7.0
	 **/
	g_object_class_install_property (gobject_class, PROP_PRIORITY,
				g_param_spec_string ("priority",
					"Priority", "The contact's importance.",
					NULL,
					G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	/**
	 * GDataContactsContact:sensitivity:
	 *
	 * The sensitivity of the contact's data. For example: %GDATA_CONTACTS_SENSITIVITY_NORMAL or %GDATA_CONTACTS_SENSITIVITY_PRIVATE.
	 *
	 * Since: 0.7.0
	 **/
	g_object_class_install_property (gobject_class, PROP_SENSITIVITY,
				g_param_spec_string ("sensitivity",
					"Sensitivity", "The sensitivity of the contact's data.",
					NULL,
					G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	/**
	 * GDataContactsContact:short-name:
	 *
	 * A short name for the contact. This should be used for contracted versions of the contact's actual name,
	 * whereas #GDataContactsContact:nickname should be used for nicknames.
	 *
	 * Since: 0.7.0
	 **/
	g_object_class_install_property (gobject_class, PROP_SHORT_NAME,
				g_param_spec_string ("short-name",
					"Short name", "A short name for the contact.",
					NULL,
					G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	/**
	 * GDataContactsContact:subject:
	 *
	 * The subject of the contact. (i.e. The contact's relevance to the address book.)
	 *
	 * Since: 0.7.0
	 **/
	g_object_class_install_property (gobject_class, PROP_SUBJECT,
				g_param_spec_string ("subject",
					"Subject", "The subject of the contact.",
					NULL,
					G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void notify_full_name_cb (GObject *gobject, GParamSpec *pspec, GDataContactsContact *self);

static void
notify_title_cb (GObject *gobject, GParamSpec *pspec, GDataContactsContact *self)
{
	/* Update GDataGDName:full-name */
	g_signal_handlers_block_by_func (self->priv->name, notify_full_name_cb, self);
	gdata_gd_name_set_full_name (self->priv->name, gdata_entry_get_title (GDATA_ENTRY (self)));
	g_signal_handlers_unblock_by_func (self->priv->name, notify_full_name_cb, self);
}

static void
notify_full_name_cb (GObject *gobject, GParamSpec *pspec, GDataContactsContact *self)
{
	/* Update GDataEntry:title */
	g_signal_handlers_block_by_func (self, notify_title_cb, self);
	gdata_entry_set_title (GDATA_ENTRY (self), gdata_gd_name_get_full_name (self->priv->name));
	g_signal_handlers_unblock_by_func (self, notify_title_cb, self);
}

static void
gdata_contacts_contact_init (GDataContactsContact *self)
{
	GDataCategory *category;

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, GDATA_TYPE_CONTACTS_CONTACT, GDataContactsContactPrivate);
	self->priv->extended_properties = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
	self->priv->groups = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

	/* Create a default name, so the name's properties can be set for a blank contact */
	self->priv->name = gdata_gd_name_new (NULL, NULL);

	/* Listen to change notifications for the entry's title, since it's linked to GDataGDName:full-name */
	g_signal_connect (self, "notify::title", (GCallback) notify_title_cb, self);
	g_signal_connect (self->priv->name, "notify::full-name", (GCallback) notify_full_name_cb, self);

	/* Initialise the contact's birthday to a sane but invalid date */
	g_date_clear (&(self->priv->birthday), 1);

	/* Add the "contact" kind category */
	category = gdata_category_new ("http://schemas.google.com/contact/2008#contact", "http://schemas.google.com/g/2005#kind", NULL);
	gdata_entry_add_category (GDATA_ENTRY (self), category);
	g_object_unref (category);
}

static void
gdata_contacts_contact_dispose (GObject *object)
{
	GDataContactsContact *self = GDATA_CONTACTS_CONTACT (object);

	if (self->priv->name != NULL)
		g_object_unref (self->priv->name);
	self->priv->name = NULL;

	gdata_contacts_contact_remove_all_organizations (self);
	gdata_contacts_contact_remove_all_email_addresses (self);
	gdata_contacts_contact_remove_all_im_addresses (self);
	gdata_contacts_contact_remove_all_postal_addresses (self);
	gdata_contacts_contact_remove_all_phone_numbers (self);
	gdata_contacts_contact_remove_all_jots (self);
	gdata_contacts_contact_remove_all_relations (self);
	gdata_contacts_contact_remove_all_websites (self);
	gdata_contacts_contact_remove_all_events (self);
	gdata_contacts_contact_remove_all_calendars (self);

	/* Chain up to the parent class */
	G_OBJECT_CLASS (gdata_contacts_contact_parent_class)->dispose (object);
}

static void
gdata_contacts_contact_finalize (GObject *object)
{
	GDataContactsContactPrivate *priv = GDATA_CONTACTS_CONTACT (object)->priv;

	g_hash_table_destroy (priv->extended_properties);
	g_hash_table_destroy (priv->groups);
	g_free (priv->photo_etag);
	g_free (priv->nickname);
	g_free (priv->billing_information);
	g_free (priv->directory_server);
	g_free (priv->gender);
	g_free (priv->initials);
	g_free (priv->maiden_name);
	g_free (priv->mileage);
	g_free (priv->occupation);
	g_free (priv->priority);
	g_free (priv->sensitivity);
	g_free (priv->short_name);
	g_free (priv->subject);

	/* Chain up to the parent class */
	G_OBJECT_CLASS (gdata_contacts_contact_parent_class)->finalize (object);
}

static void
gdata_contacts_contact_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
	GDataContactsContactPrivate *priv = GDATA_CONTACTS_CONTACT (object)->priv;

	switch (property_id) {
		case PROP_EDITED:
			g_value_set_boxed (value, &(priv->edited));
			break;
		case PROP_DELETED:
			g_value_set_boolean (value, priv->deleted);
			break;
		case PROP_HAS_PHOTO:
			g_value_set_boolean (value, (priv->photo_etag != NULL) ? TRUE : FALSE);
			break;
		case PROP_NAME:
			g_value_set_object (value, priv->name);
			break;
		case PROP_NICKNAME:
			g_value_set_string (value, priv->nickname);
			break;
		case PROP_BIRTHDAY:
			g_value_set_boxed (value, &(priv->birthday));
			break;
		case PROP_BIRTHDAY_HAS_YEAR:
			g_value_set_boolean (value, priv->birthday_has_year);
			break;
		case PROP_BILLING_INFORMATION:
			g_value_set_string (value, priv->billing_information);
			break;
		case PROP_DIRECTORY_SERVER:
			g_value_set_string (value, priv->directory_server);
			break;
		case PROP_GENDER:
			g_value_set_string (value, priv->gender);
			break;
		case PROP_INITIALS:
			g_value_set_string (value, priv->initials);
			break;
		case PROP_MAIDEN_NAME:
			g_value_set_string (value, priv->maiden_name);
			break;
		case PROP_MILEAGE:
			g_value_set_string (value, priv->mileage);
			break;
		case PROP_OCCUPATION:
			g_value_set_string (value, priv->occupation);
			break;
		case PROP_PRIORITY:
			g_value_set_string (value, priv->priority);
			break;
		case PROP_SENSITIVITY:
			g_value_set_string (value, priv->sensitivity);
			break;
		case PROP_SHORT_NAME:
			g_value_set_string (value, priv->short_name);
			break;
		case PROP_SUBJECT:
			g_value_set_string (value, priv->subject);
			break;
		default:
			/* We don't have any other property... */
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void
gdata_contacts_contact_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
	GDataContactsContact *self = GDATA_CONTACTS_CONTACT (object);

	switch (property_id) {
		case PROP_NAME:
			gdata_contacts_contact_set_name (self, g_value_get_object (value));
			break;
		case PROP_NICKNAME:
			gdata_contacts_contact_set_nickname (self, g_value_get_string (value));
			break;
		case PROP_BIRTHDAY:
			gdata_contacts_contact_set_birthday (self, g_value_get_boxed (value), self->priv->birthday_has_year);
			break;
		case PROP_BIRTHDAY_HAS_YEAR:
			gdata_contacts_contact_set_birthday (self, &(self->priv->birthday), g_value_get_boolean (value));
			break;
		case PROP_BILLING_INFORMATION:
			gdata_contacts_contact_set_billing_information (self, g_value_get_string (value));
			break;
		case PROP_DIRECTORY_SERVER:
			gdata_contacts_contact_set_directory_server (self, g_value_get_string (value));
			break;
		case PROP_GENDER:
			gdata_contacts_contact_set_gender (self, g_value_get_string (value));
			break;
		case PROP_INITIALS:
			gdata_contacts_contact_set_initials (self, g_value_get_string (value));
			break;
		case PROP_MAIDEN_NAME:
			gdata_contacts_contact_set_maiden_name (self, g_value_get_string (value));
			break;
		case PROP_MILEAGE:
			gdata_contacts_contact_set_mileage (self, g_value_get_string (value));
			break;
		case PROP_OCCUPATION:
			gdata_contacts_contact_set_occupation (self, g_value_get_string (value));
			break;
		case PROP_PRIORITY:
			gdata_contacts_contact_set_priority (self, g_value_get_string (value));
			break;
		case PROP_SENSITIVITY:
			gdata_contacts_contact_set_sensitivity (self, g_value_get_string (value));
			break;
		case PROP_SHORT_NAME:
			gdata_contacts_contact_set_short_name (self, g_value_get_string (value));
			break;
		case PROP_SUBJECT:
			gdata_contacts_contact_set_subject (self, g_value_get_string (value));
			break;
		default:
			/* We don't have any other property... */
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static gboolean
parse_xml (GDataParsable *parsable, xmlDoc *doc, xmlNode *node, gpointer user_data, GError **error)
{
	gboolean success;
	GDataContactsContact *self = GDATA_CONTACTS_CONTACT (parsable);

	if (gdata_parser_is_namespace (node, "http://www.w3.org/2007/app") == TRUE &&
	    gdata_parser_time_val_from_element (node, "edited", P_REQUIRED | P_NO_DUPES, &(self->priv->edited), &success, error) == TRUE) {
		return success;
	} else if (gdata_parser_is_namespace (node, "http://schemas.google.com/g/2005") == TRUE) {
		if (gdata_parser_object_from_element_setter (node, "email", P_REQUIRED, GDATA_TYPE_GD_EMAIL_ADDRESS,
		                                             gdata_contacts_contact_add_email_address, self, &success, error) == TRUE ||
		    gdata_parser_object_from_element_setter (node, "im", P_REQUIRED, GDATA_TYPE_GD_IM_ADDRESS,
		                                             gdata_contacts_contact_add_im_address, self, &success, error) == TRUE ||
		    gdata_parser_object_from_element_setter (node, "phoneNumber", P_REQUIRED, GDATA_TYPE_GD_PHONE_NUMBER,
		                                             gdata_contacts_contact_add_phone_number, self, &success, error) == TRUE ||
		    gdata_parser_object_from_element_setter (node, "structuredPostalAddress", P_REQUIRED, GDATA_TYPE_GD_POSTAL_ADDRESS,
		                                             gdata_contacts_contact_add_postal_address, self, &success, error) == TRUE ||
		    gdata_parser_object_from_element_setter (node, "organization", P_REQUIRED, GDATA_TYPE_GD_ORGANIZATION,
		                                             gdata_contacts_contact_add_organization, self, &success, error) == TRUE ||
		    gdata_parser_object_from_element (node, "name", P_REQUIRED, GDATA_TYPE_GD_NAME, &(self->priv->name), &success, error) == TRUE) {
			return success;
		} else if (xmlStrcmp (node->name, (xmlChar*) "extendedProperty") == 0) {
			/* gd:extendedProperty */
			xmlChar *name, *value;
			xmlBuffer *buffer = NULL;

			name = xmlGetProp (node, (xmlChar*) "name");
			if (name == NULL)
				return gdata_parser_error_required_property_missing (node, "name", error);

			/* Get either the value property, or the element's content */
			value = xmlGetProp (node, (xmlChar*) "value");
			if (value == NULL) {
				xmlNode *child_node;

				/* Use the element's content instead (arbitrary XML) */
				buffer = xmlBufferCreate ();
				for (child_node = node->children; child_node != NULL; child_node = child_node->next)
					xmlNodeDump (buffer, doc, child_node, 0, 0);
				value = (xmlChar*) xmlBufferContent (buffer);
			}

			gdata_contacts_contact_set_extended_property (self, (gchar*) name, (gchar*) value);

			if (buffer != NULL)
				xmlBufferFree (buffer);
			else
				g_free (value);
		} else if (xmlStrcmp (node->name, (xmlChar*) "deleted") == 0) {
			/* gd:deleted */
			self->priv->deleted = TRUE;
		} else {
			return GDATA_PARSABLE_CLASS (gdata_contacts_contact_parent_class)->parse_xml (parsable, doc, node, user_data, error);
		}
	} else if (gdata_parser_is_namespace (node, "http://schemas.google.com/contact/2008") == TRUE) {
		if (gdata_parser_object_from_element_setter (node, "jot", P_REQUIRED, GDATA_TYPE_GCONTACT_JOT,
		                                             gdata_contacts_contact_add_jot, self, &success, error) == TRUE ||
		    gdata_parser_object_from_element_setter (node, "relation", P_REQUIRED, GDATA_TYPE_GCONTACT_RELATION,
		                                             gdata_contacts_contact_add_relation, self, &success, error) == TRUE ||
		    gdata_parser_object_from_element_setter (node, "event", P_REQUIRED, GDATA_TYPE_GCONTACT_EVENT,
		                                             gdata_contacts_contact_add_event, self, &success, error) == TRUE ||
		    gdata_parser_object_from_element_setter (node, "website", P_REQUIRED, GDATA_TYPE_GCONTACT_WEBSITE,
		                                             gdata_contacts_contact_add_website, self, &success, error) == TRUE ||
		    gdata_parser_object_from_element_setter (node, "calendarLink", P_REQUIRED, GDATA_TYPE_GCONTACT_CALENDAR,
		                                             gdata_contacts_contact_add_calendar, self, &success, error) == TRUE ||
		    gdata_parser_string_from_element (node, "nickname", P_REQUIRED | P_NO_DUPES, &(self->priv->nickname), &success, error) == TRUE ||
		    gdata_parser_string_from_element (node, "billingInformation", P_REQUIRED | P_NO_DUPES | P_NON_EMPTY,
		                                      &(self->priv->billing_information), &success, error) == TRUE ||
		    gdata_parser_string_from_element (node, "directoryServer", P_REQUIRED | P_NO_DUPES | P_NON_EMPTY,
		                                      &(self->priv->directory_server), &success, error) == TRUE ||
		    gdata_parser_string_from_element (node, "initials", P_REQUIRED | P_NO_DUPES, &(self->priv->initials), &success, error) == TRUE ||
		    gdata_parser_string_from_element (node, "maidenName", P_REQUIRED | P_NO_DUPES,
		                                      &(self->priv->maiden_name), &success, error) == TRUE ||
		    gdata_parser_string_from_element (node, "mileage", P_REQUIRED | P_NO_DUPES, &(self->priv->mileage), &success, error) == TRUE ||
		    gdata_parser_string_from_element (node, "occupation", P_REQUIRED | P_NO_DUPES,
		                                      &(self->priv->occupation), &success, error) == TRUE ||
		    gdata_parser_string_from_element (node, "shortName", P_REQUIRED | P_NO_DUPES,
		                                      &(self->priv->short_name), &success, error) == TRUE ||
		    gdata_parser_string_from_element (node, "subject", P_REQUIRED | P_NO_DUPES, &(self->priv->subject), &success, error) == TRUE) {
			return success;
		} else if (xmlStrcmp (node->name, (xmlChar*) "gender") == 0) {
			/* gContact:gender */
			xmlChar *value;

			if (self->priv->gender != NULL)
				return gdata_parser_error_duplicate_element (node, error);

			value = xmlGetProp (node, (xmlChar*) "value");
			if (value == NULL || *value == '\0') {
				xmlFree (value);
				return gdata_parser_error_required_content_missing (node, error);
			}

			self->priv->gender = (gchar*) value;
		} else if (xmlStrcmp (node->name, (xmlChar*) "priority") == 0) {
			/* gContact:priority */
			xmlChar *rel;

			if (self->priv->priority != NULL)
				return gdata_parser_error_duplicate_element (node, error);

			rel = xmlGetProp (node, (xmlChar*) "rel");
			if (rel == NULL || *rel == '\0') {
				xmlFree (rel);
				return gdata_parser_error_required_content_missing (node, error);
			}

			self->priv->priority = (gchar*) rel;
		} else if (xmlStrcmp (node->name, (xmlChar*) "sensitivity") == 0) {
			/* gContact:sensitivity */
			xmlChar *rel;

			if (self->priv->sensitivity != NULL)
				return gdata_parser_error_duplicate_element (node, error);

			rel = xmlGetProp (node, (xmlChar*) "rel");
			if (rel == NULL || *rel == '\0') {
				xmlFree (rel);
				return gdata_parser_error_required_content_missing (node, error);
			}

			self->priv->sensitivity = (gchar*) rel;
		} else if (xmlStrcmp (node->name, (xmlChar*) "groupMembershipInfo") == 0) {
			/* gContact:groupMembershipInfo */
			xmlChar *href;
			gboolean deleted_bool;

			href = xmlGetProp (node, (xmlChar*) "href");
			if (href == NULL)
				return gdata_parser_error_required_property_missing (node, "href", error);

			/* Has it been deleted? */
			if (gdata_parser_boolean_from_property (node, "deleted", &deleted_bool, 0, error) == FALSE)
				return FALSE;

			/* Insert it into the hash table */
			g_hash_table_insert (self->priv->groups, (gchar*) href, GUINT_TO_POINTER (deleted_bool));
		} else if (xmlStrcmp (node->name, (xmlChar*) "birthday") == 0) {
			/* gContact:birthday */
			xmlChar *birthday;
			guint length = 0, year = 666, month, day;

			if (g_date_valid (&(self->priv->birthday)) == TRUE)
				return gdata_parser_error_duplicate_element (node, error);

			birthday = xmlGetProp (node, (xmlChar*) "when");
			if (birthday == NULL)
				return gdata_parser_error_required_property_missing (node, "when", error);
			length = strlen ((char*) birthday);

			/* Try parsing the two possible formats: YYYY-MM-DD and --MM-DD */
			if (((length == 10 && sscanf ((char*) birthday, "%4u-%2u-%2u", &year, &month, &day) == 3) ||
			     (length == 7 && sscanf ((char*) birthday, "--%2u-%2u", &month, &day) == 2)) &&
			    g_date_valid_dmy (day, month, year) == TRUE) {
				/* Store the values in the GDate */
				g_date_set_dmy (&(self->priv->birthday), day, month, year);
				self->priv->birthday_has_year = (length == 10) ? TRUE : FALSE;
				xmlFree (birthday);
			} else {
				/* Parsing failed */
				gdata_parser_error_not_iso8601_format (node, (gchar*) birthday, error);
				xmlFree (birthday);
				return FALSE;
			}
		} else {
			return GDATA_PARSABLE_CLASS (gdata_contacts_contact_parent_class)->parse_xml (parsable, doc, node, user_data, error);
		}
	} else {
		/* If we haven't yet found a photo, check to see if it's a photo <link> element */
		if (self->priv->photo_etag == NULL && xmlStrcmp (node->name, (xmlChar*) "link") == 0) {
			xmlChar *rel = xmlGetProp (node, (xmlChar*) "rel");
			if (xmlStrcmp (rel, (xmlChar*) "http://schemas.google.com/contacts/2008/rel#photo") == 0) {
				/* It's the photo link (http://code.google.com/apis/contacts/docs/2.0/reference.html#Photos), whose ETag we should
				 * note down, then pass onto the parent class to parse properly */
				self->priv->photo_etag = (gchar*) xmlGetProp (node, (xmlChar*) "etag");
			}
			xmlFree (rel);
		}

		return GDATA_PARSABLE_CLASS (gdata_contacts_contact_parent_class)->parse_xml (parsable, doc, node, user_data, error);
	}

	return TRUE;
}

static void
get_child_xml (GList *list, GString *xml_string)
{
	GList *i;

	for (i = list; i != NULL; i = i->next)
		_gdata_parsable_get_xml (GDATA_PARSABLE (i->data), xml_string, FALSE);
}

static void
get_extended_property_xml_cb (const gchar *name, const gchar *value, GString *xml_string)
{
	g_string_append_printf (xml_string, "<gd:extendedProperty name='%s'>%s</gd:extendedProperty>", name, value);
}

static void
get_group_xml_cb (const gchar *href, gpointer deleted, GString *xml_string)
{
	g_string_append_printf (xml_string, "<gContact:groupMembershipInfo href='%s'/>", href);
}

static void
get_xml (GDataParsable *parsable, GString *xml_string)
{
	GDataContactsContactPrivate *priv = GDATA_CONTACTS_CONTACT (parsable)->priv;

	/* Chain up to the parent class */
	GDATA_PARSABLE_CLASS (gdata_contacts_contact_parent_class)->get_xml (parsable, xml_string);

	/* Name */
	_gdata_parsable_get_xml (GDATA_PARSABLE (priv->name), xml_string, FALSE);

	/* Lists of stuff */
	get_child_xml (priv->email_addresses, xml_string);
	get_child_xml (priv->im_addresses, xml_string);
	get_child_xml (priv->phone_numbers, xml_string);
	get_child_xml (priv->postal_addresses, xml_string);
	get_child_xml (priv->organizations, xml_string);
	get_child_xml (priv->jots, xml_string);
	get_child_xml (priv->relations, xml_string);
	get_child_xml (priv->websites, xml_string);
	get_child_xml (priv->events, xml_string);
	get_child_xml (priv->calendars, xml_string);

	/* Extended properties */
	g_hash_table_foreach (priv->extended_properties, (GHFunc) get_extended_property_xml_cb, xml_string);

	/* Group membership info */
	g_hash_table_foreach (priv->groups, (GHFunc) get_group_xml_cb, xml_string);

	/* gContact:nickname */
	if (priv->nickname != NULL)
		gdata_parser_string_append_escaped (xml_string, "<gContact:nickname>", priv->nickname, "</gContact:nickname>");

	/* gContact:birthday */
	if (g_date_valid (&(priv->birthday)) == TRUE) {
		if (priv->birthday_has_year == TRUE) {
			g_string_append_printf (xml_string, "<gContact:birthday when='%04u-%02u-%02u'/>",
			                        g_date_get_year (&(priv->birthday)),
			                        g_date_get_month (&(priv->birthday)),
			                        g_date_get_day (&(priv->birthday)));
		} else {
			g_string_append_printf (xml_string, "<gContact:birthday when='--%02u-%02u'/>",
			                        g_date_get_month (&(priv->birthday)),
			                        g_date_get_day (&(priv->birthday)));
		}
	}

	/* gContact:billingInformation */
	if (priv->billing_information != NULL) {
		gdata_parser_string_append_escaped (xml_string,
		                                    "<gContact:billingInformation>", priv->billing_information, "</gContact:billingInformation>");
	}

	/* gContact:directoryServer */
	if (priv->directory_server != NULL)
		gdata_parser_string_append_escaped (xml_string, "<gContact:directoryServer>", priv->directory_server, "</gContact:directoryServer>");

	/* gContact:gender */
	if (priv->gender != NULL)
		gdata_parser_string_append_escaped (xml_string, "<gContact:gender value='", priv->gender, "'/>");

	/* gContact:initials */
	if (priv->initials != NULL)
		gdata_parser_string_append_escaped (xml_string, "<gContact:initials>", priv->initials, "</gContact:initials>");

	/* gContact:maidenName */
	if (priv->maiden_name != NULL)
		gdata_parser_string_append_escaped (xml_string, "<gContact:maidenName>", priv->maiden_name, "</gContact:maidenName>");

	/* gContact:mileage */
	if (priv->mileage != NULL)
		gdata_parser_string_append_escaped (xml_string, "<gContact:mileage>", priv->mileage, "</gContact:mileage>");

	/* gContact:occupation */
	if (priv->occupation != NULL)
		gdata_parser_string_append_escaped (xml_string, "<gContact:occupation>", priv->occupation, "</gContact:occupation>");

	/* gContact:priority */
	if (priv->priority != NULL)
		gdata_parser_string_append_escaped (xml_string, "<gContact:priority rel='", priv->priority, "'/>");

	/* gContact:sensitivity */
	if (priv->sensitivity != NULL)
		gdata_parser_string_append_escaped (xml_string, "<gContact:sensitivity rel='", priv->sensitivity, "'/>");

	/* gContact:shortName */
	if (priv->short_name != NULL)
		gdata_parser_string_append_escaped (xml_string, "<gContact:shortName>", priv->short_name, "</gContact:shortName>");

	/* gContact:subject */
	if (priv->subject != NULL)
		gdata_parser_string_append_escaped (xml_string, "<gContact:subject>", priv->subject, "</gContact:subject>");

	/* TODO:
	 * - Finish supporting all tags
	 * - Check things are escaped (or not) as appropriate
	 */
}

static void
get_namespaces (GDataParsable *parsable, GHashTable *namespaces)
{
	/* Chain up to the parent class */
	GDATA_PARSABLE_CLASS (gdata_contacts_contact_parent_class)->get_namespaces (parsable, namespaces);

	g_hash_table_insert (namespaces, (gchar*) "gd", (gchar*) "http://schemas.google.com/g/2005");
	g_hash_table_insert (namespaces, (gchar*) "gContact", (gchar*) "http://schemas.google.com/contact/2008");
	g_hash_table_insert (namespaces, (gchar*) "app", (gchar*) "http://www.w3.org/2007/app");
}

static gchar *
get_entry_uri (const gchar *id)
{
	const gchar *base_pos;
	gchar *uri = g_strdup (id);

	/* The service API sometimes stubbornly insists on using the "base" view instead of the "full" view, which we have
	 * to fix, or our extended attributes are never visible */
	base_pos = strstr (uri, "/base/");
	if (base_pos != NULL)
		memcpy ((char*) base_pos, "/full/", 6);

	return uri;
}

/**
 * gdata_contacts_contact_new:
 * @id: the contact's ID, or %NULL
 *
 * Creates a new #GDataContactsContact with the given ID and default properties.
 *
 * Return value: a new #GDataContactsContact; unref with g_object_unref()
 *
 * Since: 0.2.0
 **/
GDataContactsContact *
gdata_contacts_contact_new (const gchar *id)
{
	GDataContactsContact *contact = GDATA_CONTACTS_CONTACT (g_object_new (GDATA_TYPE_CONTACTS_CONTACT, "id", id, NULL));

	/* Set the edited property to the current time (creation time). We don't do this in *_init() since that would cause
	 * setting it from parse_xml() to fail (duplicate element). */
	g_get_current_time (&(contact->priv->edited));

	return contact;
}

/**
 * gdata_contacts_contact_get_edited:
 * @self: a #GDataContactsContact
 * @edited: a #GTimeVal
 *
 * Gets the #GDataContactsContact:edited property and puts it in @edited. If the property is unset,
 * both fields in the #GTimeVal will be set to <code class="literal">0</code>.
 *
 * Since: 0.2.0
 **/
void
gdata_contacts_contact_get_edited (GDataContactsContact *self, GTimeVal *edited)
{
	g_return_if_fail (GDATA_IS_CONTACTS_CONTACT (self));
	g_return_if_fail (edited != NULL);
	*edited = self->priv->edited;
}

/**
 * gdata_contacts_contact_get_name:
 * @self: a #GDataContactsContact
 *
 * Gets the #GDataContactsContact:name property.
 *
 * Return value: the contact's name, or %NULL
 *
 * Since: 0.5.0
 **/
GDataGDName *
gdata_contacts_contact_get_name (GDataContactsContact *self)
{
	g_return_val_if_fail (GDATA_IS_CONTACTS_CONTACT (self), NULL);
	return self->priv->name;
}

/**
 * gdata_contacts_contact_set_name:
 * @self: a #GDataContactsContact
 * @name: the new #GDataGDName
 *
 * Sets the #GDataContactsContact:name property to @name, and increments its reference count.
 *
 * @name must not be %NULL, though all its properties may be %NULL.
 *
 * Since: 0.7.0
 **/
void
gdata_contacts_contact_set_name (GDataContactsContact *self, GDataGDName *name)
{
	g_return_if_fail (GDATA_IS_CONTACTS_CONTACT (self));
	g_return_if_fail (GDATA_IS_GD_NAME (name));

	if (self->priv->name != NULL)
		g_object_unref (self->priv->name);
	self->priv->name = g_object_ref (name);
	g_object_notify (G_OBJECT (self), "name");

	/* Notify the change in #GDataGDName:full-name explicitly, so that our #GDataEntry:title gets updated */
	notify_full_name_cb (G_OBJECT (name), NULL, self);
}

/**
 * gdata_contacts_contact_get_nickname:
 * @self: a #GDataContactsContact
 *
 * Gets the #GDataContactsContact:nickname property.
 *
 * Return value: the contact's nickname, or %NULL
 *
 * Since: 0.7.0
 **/
const gchar *
gdata_contacts_contact_get_nickname (GDataContactsContact *self)
{
	g_return_val_if_fail (GDATA_IS_CONTACTS_CONTACT (self), NULL);
	return self->priv->nickname;
}

/**
 * gdata_contacts_contact_set_nickname:
 * @self: a #GDataContactsContact
 * @nickname: the new nickname, or %NULL
 *
 * Sets the #GDataContactsContact:nickname property to @nickname.
 *
 * If @nickname is %NULL, the contact's nickname will be removed.
 *
 * Since: 0.7.0
 **/
void
gdata_contacts_contact_set_nickname (GDataContactsContact *self, const gchar *nickname)
{
	g_return_if_fail (GDATA_IS_CONTACTS_CONTACT (self));

	g_free (self->priv->nickname);
	self->priv->nickname = g_strdup (nickname);
	g_object_notify (G_OBJECT (self), "nickname");
}

/**
 * gdata_contacts_contact_get_birthday:
 * @self: a #GDataContactsContact
 * @birthday: return location for the birthday, or %NULL
 *
 * Gets the #GDataContactsContact:birthday and #GDataContactsContact:birthday-has-year properties. If @birthday is non-%NULL,
 * #GDataContactsContact:birthday is returned in it. The function returns the value of #GDataContactsContact:birthday-has-year,
 * which specifies whether the year in @birthday is meaningful. Contacts may not have the year of their birth set, in which
 * case, the function would return %FALSE, and the year in @birthday should be ignored.
 *
 * Return value: whether the contact's birthday has the year set
 *
 * Since: 0.7.0
 **/
gboolean
gdata_contacts_contact_get_birthday (GDataContactsContact *self, GDate *birthday)
{
	g_return_val_if_fail (GDATA_IS_CONTACTS_CONTACT (self), FALSE);

	if (birthday != NULL)
		*birthday = self->priv->birthday;
	return self->priv->birthday_has_year;
}

/**
 * gdata_contacts_contact_set_birthday:
 * @self: a #GDataContactsContact
 * @birthday: the new birthday, or %NULL
 * @birthday_has_year: %TRUE if @birthday's year is relevant, %FALSE otherwise
 *
 * Sets the #GDataContactsContact:birthday property to @birthday and the #GDataContactsContact:birthday-has-year property to @birthday_has_year.
 * See gdata_contacts_contact_get_birthday() for an explanation of the interation between these two properties.
 *
 * If @birthday is %NULL, the contact's birthday will be removed.
 *
 * Since: 0.7.0
 **/
void
gdata_contacts_contact_set_birthday (GDataContactsContact *self, GDate *birthday, gboolean birthday_has_year)
{
	g_return_if_fail (GDATA_IS_CONTACTS_CONTACT (self));
	g_return_if_fail (birthday == NULL || g_date_valid (birthday));

	if (birthday != NULL)
		self->priv->birthday = *birthday;
	else
		g_date_clear (&(self->priv->birthday), 1);

	self->priv->birthday_has_year = birthday_has_year;

	g_object_freeze_notify (G_OBJECT (self));
	g_object_notify (G_OBJECT (self), "birthday");
	g_object_notify (G_OBJECT (self), "birthday-has-year");
	g_object_thaw_notify (G_OBJECT (self));
}

/**
 * gdata_contacts_contact_get_billing_information:
 * @self: a #GDataContactsContact
 *
 * Gets the #GDataContactsContact:billing-information property.
 *
 * Return value: the contact's billing information, or %NULL
 *
 * Since: 0.7.0
 **/
const gchar *
gdata_contacts_contact_get_billing_information (GDataContactsContact *self)
{
	g_return_val_if_fail (GDATA_IS_CONTACTS_CONTACT (self), NULL);
	return self->priv->billing_information;
}

/**
 * gdata_contacts_contact_set_billing_information:
 * @self: a #GDataContactsContact
 * @billing_information: the new billing information for the contact, or %NULL
 *
 * Sets the #GDataContactsContact:billing-information property to @billing_information.
 *
 * If @billing_information is %NULL, the contact's billing information will be removed.
 *
 * Since: 0.7.0
 **/
void
gdata_contacts_contact_set_billing_information (GDataContactsContact *self, const gchar *billing_information)
{
	g_return_if_fail (GDATA_IS_CONTACTS_CONTACT (self));
	g_return_if_fail (billing_information == NULL || *billing_information != '\0');

	g_free (self->priv->billing_information);
	self->priv->billing_information = g_strdup (billing_information);
	g_object_notify (G_OBJECT (self), "billing-information");
}

/**
 * gdata_contacts_contact_get_directory_server:
 * @self: a #GDataContactsContact
 *
 * Gets the #GDataContactsContact:directory-server property.
 *
 * Return value: the name or address of a directory server associated with the contact, or %NULL
 *
 * Since: 0.7.0
 **/
const gchar *
gdata_contacts_contact_get_directory_server (GDataContactsContact *self)
{
	g_return_val_if_fail (GDATA_IS_CONTACTS_CONTACT (self), NULL);
	return self->priv->directory_server;
}

/**
 * gdata_contacts_contact_set_directory_server:
 * @self: a #GDataContactsContact
 * @directory_server: the new name or address of a directory server associated with the contact, or %NULL
 *
 * Sets the #GDataContactsContact:directory-server property to @directory_server.
 *
 * If @directory_server is %NULL, the contact's directory server will be removed.
 *
 * Since: 0.7.0
 **/
void
gdata_contacts_contact_set_directory_server (GDataContactsContact *self, const gchar *directory_server)
{
	g_return_if_fail (GDATA_IS_CONTACTS_CONTACT (self));
	g_return_if_fail (directory_server == NULL || *directory_server != '\0');

	g_free (self->priv->directory_server);
	self->priv->directory_server = g_strdup (directory_server);
	g_object_notify (G_OBJECT (self), "directory-server");
}

/**
 * gdata_contacts_contact_get_gender:
 * @self: a #GDataContactsContact
 *
 * Gets the #GDataContactsContact:gender property.
 *
 * Return value: the gender of the contact, or %NULL
 *
 * Since: 0.7.0
 **/
const gchar *
gdata_contacts_contact_get_gender (GDataContactsContact *self)
{
	g_return_val_if_fail (GDATA_IS_CONTACTS_CONTACT (self), NULL);
	return self->priv->gender;
}

/**
 * gdata_contacts_contact_set_gender:
 * @self: a #GDataContactsContact
 * @gender: the new gender of the contact, or %NULL
 *
 * Sets the #GDataContactsContact:gender property to @gender.
 *
 * If @gender is %NULL, the contact's gender will be removed.
 *
 * Since: 0.7.0
 **/
void
gdata_contacts_contact_set_gender (GDataContactsContact *self, const gchar *gender)
{
	g_return_if_fail (GDATA_IS_CONTACTS_CONTACT (self));
	g_return_if_fail (gender == NULL || *gender != '\0');

	g_free (self->priv->gender);
	self->priv->gender = g_strdup (gender);
	g_object_notify (G_OBJECT (self), "gender");
}

/**
 * gdata_contacts_contact_get_initials:
 * @self: a #GDataContactsContact
 *
 * Gets the #GDataContactsContact:initials property.
 *
 * Return value: the initials of the contact, or %NULL
 *
 * Since: 0.7.0
 **/
const gchar *
gdata_contacts_contact_get_initials (GDataContactsContact *self)
{
	g_return_val_if_fail (GDATA_IS_CONTACTS_CONTACT (self), NULL);
	return self->priv->initials;
}

/**
 * gdata_contacts_contact_set_initials:
 * @self: a #GDataContactsContact
 * @initials: the new initials of the contact, or %NULL
 *
 * Sets the #GDataContactsContact:initials property to @initials.
 *
 * If @initials is %NULL, the contact's initials will be removed.
 *
 * Since: 0.7.0
 **/
void
gdata_contacts_contact_set_initials (GDataContactsContact *self, const gchar *initials)
{
	g_return_if_fail (GDATA_IS_CONTACTS_CONTACT (self));

	g_free (self->priv->initials);
	self->priv->initials = g_strdup (initials);
	g_object_notify (G_OBJECT (self), "initials");
}

/**
 * gdata_contacts_contact_get_maiden_name:
 * @self: a #GDataContactsContact
 *
 * Gets the #GDataContactsContact:maiden-name property.
 *
 * Return value: the maiden name of the contact, or %NULL
 *
 * Since: 0.7.0
 **/
const gchar *
gdata_contacts_contact_get_maiden_name (GDataContactsContact *self)
{
	g_return_val_if_fail (GDATA_IS_CONTACTS_CONTACT (self), NULL);
	return self->priv->maiden_name;
}

/**
 * gdata_contacts_contact_set_maiden_name:
 * @self: a #GDataContactsContact
 * @maiden_name: the new maiden name of the contact, or %NULL
 *
 * Sets the #GDataContactsContact:maiden-name property to @maiden_name.
 *
 * If @maiden_name is %NULL, the contact's maiden name will be removed.
 *
 * Since: 0.7.0
 **/
void
gdata_contacts_contact_set_maiden_name (GDataContactsContact *self, const gchar *maiden_name)
{
	g_return_if_fail (GDATA_IS_CONTACTS_CONTACT (self));

	g_free (self->priv->maiden_name);
	self->priv->maiden_name = g_strdup (maiden_name);
	g_object_notify (G_OBJECT (self), "maiden-name");
}

/**
 * gdata_contacts_contact_get_mileage:
 * @self: a #GDataContactsContact
 *
 * Gets the #GDataContactsContact:mileage property.
 *
 * Return value: a mileage associated with the contact, or %NULL
 *
 * Since: 0.7.0
 **/
const gchar *
gdata_contacts_contact_get_mileage (GDataContactsContact *self)
{
	g_return_val_if_fail (GDATA_IS_CONTACTS_CONTACT (self), NULL);
	return self->priv->mileage;
}

/**
 * gdata_contacts_contact_set_mileage:
 * @self: a #GDataContactsContact
 * @mileage: the new mileage associated with the contact, or %NULL
 *
 * Sets the #GDataContactsContact:mileage property to @mileage.
 *
 * If @mileage is %NULL, the contact's mileage will be removed.
 *
 * Since: 0.7.0
 **/
void
gdata_contacts_contact_set_mileage (GDataContactsContact *self, const gchar *mileage)
{
	g_return_if_fail (GDATA_IS_CONTACTS_CONTACT (self));

	g_free (self->priv->mileage);
	self->priv->mileage = g_strdup (mileage);
	g_object_notify (G_OBJECT (self), "mileage");
}

/**
 * gdata_contacts_contact_get_occupation:
 * @self: a #GDataContactsContact
 *
 * Gets the #GDataContactsContact:occupation property.
 *
 * Return value: the contact's occupation, or %NULL
 *
 * Since: 0.7.0
 **/
const gchar *
gdata_contacts_contact_get_occupation (GDataContactsContact *self)
{
	g_return_val_if_fail (GDATA_IS_CONTACTS_CONTACT (self), NULL);
	return self->priv->occupation;
}

/**
 * gdata_contacts_contact_set_occupation:
 * @self: a #GDataContactsContact
 * @occupation: the contact's new occupation, or %NULL
 *
 * Sets the #GDataContactsContact:occupation property to @occupation.
 *
 * If @occupation is %NULL, the contact's occupation will be removed.
 *
 * Since: 0.7.0
 **/
void
gdata_contacts_contact_set_occupation (GDataContactsContact *self, const gchar *occupation)
{
	g_return_if_fail (GDATA_IS_CONTACTS_CONTACT (self));

	g_free (self->priv->occupation);
	self->priv->occupation = g_strdup (occupation);
	g_object_notify (G_OBJECT (self), "occupation");
}

/**
 * gdata_contacts_contact_get_priority:
 * @self: a #GDataContactsContact
 *
 * Gets the #GDataContactsContact:priority property.
 *
 * Return value: the contact's priority, or %NULL
 *
 * Since: 0.7.0
 **/
const gchar *
gdata_contacts_contact_get_priority (GDataContactsContact *self)
{
	g_return_val_if_fail (GDATA_IS_CONTACTS_CONTACT (self), NULL);
	return self->priv->priority;
}

/**
 * gdata_contacts_contact_set_priority:
 * @self: a #GDataContactsContact
 * @priority: the contact's new priority, or %NULL
 *
 * Sets the #GDataContactsContact:priority property to @priority.
 *
 * If @priority is %NULL, the contact's priority will be removed.
 *
 * Since: 0.7.0
 **/
void
gdata_contacts_contact_set_priority (GDataContactsContact *self, const gchar *priority)
{
	g_return_if_fail (GDATA_IS_CONTACTS_CONTACT (self));
	g_return_if_fail (priority == NULL || *priority != '\0');

	g_free (self->priv->priority);
	self->priv->priority = g_strdup (priority);
	g_object_notify (G_OBJECT (self), "priority");
}

/**
 * gdata_contacts_contact_get_sensitivity:
 * @self: a #GDataContactsContact
 *
 * Gets the #GDataContactsContact:sensitivity property.
 *
 * Return value: the contact's sensitivity, or %NULL
 *
 * Since: 0.7.0
 **/
const gchar *
gdata_contacts_contact_get_sensitivity (GDataContactsContact *self)
{
	g_return_val_if_fail (GDATA_IS_CONTACTS_CONTACT (self), NULL);
	return self->priv->sensitivity;
}

/**
 * gdata_contacts_contact_set_sensitivity:
 * @self: a #GDataContactsContact
 * @sensitivity: the contact's new sensitivity, or %NULL
 *
 * Sets the #GDataContactsContact:sensitivity property to @sensitivity.
 *
 * If @sensitivity is %NULL, the contact's sensitivity will be removed.
 *
 * Since: 0.7.0
 **/
void
gdata_contacts_contact_set_sensitivity (GDataContactsContact *self, const gchar *sensitivity)
{
	g_return_if_fail (GDATA_IS_CONTACTS_CONTACT (self));
	g_return_if_fail (sensitivity == NULL || *sensitivity != '\0');

	g_free (self->priv->sensitivity);
	self->priv->sensitivity = g_strdup (sensitivity);
	g_object_notify (G_OBJECT (self), "sensitivity");
}

/**
 * gdata_contacts_contact_get_short_name:
 * @self: a #GDataContactsContact
 *
 * Gets the #GDataContactsContact:short-name property.
 *
 * Return value: the contact's short name, or %NULL
 *
 * Since: 0.7.0
 **/
const gchar *
gdata_contacts_contact_get_short_name (GDataContactsContact *self)
{
	g_return_val_if_fail (GDATA_IS_CONTACTS_CONTACT (self), NULL);
	return self->priv->short_name;
}

/**
 * gdata_contacts_contact_set_short_name:
 * @self: a #GDataContactsContact
 * @short_name: the contact's new short name, or %NULL
 *
 * Sets the #GDataContactsContact:short-name property to @short_name.
 *
 * If @short_name is %NULL, the contact's short name will be removed.
 *
 * Since: 0.7.0
 **/
void
gdata_contacts_contact_set_short_name (GDataContactsContact *self, const gchar *short_name)
{
	g_return_if_fail (GDATA_IS_CONTACTS_CONTACT (self));

	g_free (self->priv->short_name);
	self->priv->short_name = g_strdup (short_name);
	g_object_notify (G_OBJECT (self), "short-name");
}

/**
 * gdata_contacts_contact_get_subject:
 * @self: a #GDataContactsContact
 *
 * Gets the #GDataContactsContact:subject property.
 *
 * Return value: the contact's subject, or %NULL
 *
 * Since: 0.7.0
 **/
const gchar *
gdata_contacts_contact_get_subject (GDataContactsContact *self)
{
	g_return_val_if_fail (GDATA_IS_CONTACTS_CONTACT (self), NULL);
	return self->priv->subject;
}

/**
 * gdata_contacts_contact_set_subject:
 * @self: a #GDataContactsContact
 * @subject: the contact's new subject, or %NULL
 *
 * Sets the #GDataContactsContact:subject property to @subject.
 *
 * If @subject is %NULL, the contact's subject will be removed.
 *
 * Since: 0.7.0
 **/
void
gdata_contacts_contact_set_subject (GDataContactsContact *self, const gchar *subject)
{
	g_return_if_fail (GDATA_IS_CONTACTS_CONTACT (self));

	g_free (self->priv->subject);
	self->priv->subject = g_strdup (subject);
	g_object_notify (G_OBJECT (self), "subject");
}

/**
 * gdata_contacts_contact_add_email_address:
 * @self: a #GDataContactsContact
 * @email_address: a #GDataGDEmailAddress to add
 *
 * Adds an e-mail address to the contact's list of e-mail addresses and increments its reference count.
 *
 * Note that only one e-mail address per contact may be marked as "primary". Insertion and update operations
 * (with gdata_contacts_service_insert_contact()) will return an error if more than one e-mail address
 * is marked as primary.
 *
 * Duplicate e-mail addresses will not be added to the list.
 *
 * Since: 0.2.0
 **/
void
gdata_contacts_contact_add_email_address (GDataContactsContact *self, GDataGDEmailAddress *email_address)
{
	g_return_if_fail (GDATA_IS_CONTACTS_CONTACT (self));
	g_return_if_fail (GDATA_IS_GD_EMAIL_ADDRESS (email_address));

	if (g_list_find_custom (self->priv->email_addresses, email_address, (GCompareFunc) gdata_gd_email_address_compare) == NULL)
		self->priv->email_addresses = g_list_append (self->priv->email_addresses, g_object_ref (email_address));
}

/**
 * gdata_contacts_contact_get_email_addresses:
 * @self: a #GDataContactsContact
 *
 * Gets a list of the e-mail addresses owned by the contact.
 *
 * Return value: a #GList of #GDataGDEmailAddress<!-- -->es, or %NULL
 *
 * Since: 0.2.0
 **/
GList *
gdata_contacts_contact_get_email_addresses (GDataContactsContact *self)
{
	g_return_val_if_fail (GDATA_IS_CONTACTS_CONTACT (self), NULL);
	return self->priv->email_addresses;
}

/**
 * gdata_contacts_contact_get_primary_email_address:
 * @self: a #GDataContactsContact
 *
 * Gets the contact's primary e-mail address, if one exists.
 *
 * Return value: a #GDataGDEmailAddress, or %NULL
 *
 * Since: 0.2.0
 **/
GDataGDEmailAddress *
gdata_contacts_contact_get_primary_email_address (GDataContactsContact *self)
{
	GList *i;

	g_return_val_if_fail (GDATA_IS_CONTACTS_CONTACT (self), NULL);

	for (i = self->priv->email_addresses; i != NULL; i = i->next) {
		if (gdata_gd_email_address_is_primary (GDATA_GD_EMAIL_ADDRESS (i->data)) == TRUE)
			return GDATA_GD_EMAIL_ADDRESS (i->data);
	}

	return NULL;
}

/**
 * gdata_contacts_contact_remove_all_email_addresses:
 * @self: a #GDataContactsContact
 *
 * Removes all e-mail addresses from the contact.
 *
 * Since: 0.4.0
 **/
void
gdata_contacts_contact_remove_all_email_addresses (GDataContactsContact *self)
{
	GDataContactsContactPrivate *priv = self->priv;

	g_return_if_fail (GDATA_IS_CONTACTS_CONTACT (self));

	if (priv->email_addresses != NULL) {
		g_list_foreach (priv->email_addresses, (GFunc) g_object_unref, NULL);
		g_list_free (priv->email_addresses);
	}
	priv->email_addresses = NULL;
}

/**
 * gdata_contacts_contact_add_im_address:
 * @self: a #GDataContactsContact
 * @im_address: a #GDataGDIMAddress to add
 *
 * Adds an IM (instant messaging) address to the contact's list of IM addresses and increments its reference count.
 *
 * Note that only one IM address per contact may be marked as "primary". Insertion and update operations
 * (with gdata_contacts_service_insert_contact()) will return an error if more than one IM address
 * is marked as primary.
 *
 * Duplicate IM addresses will not be added to the list.
 *
 * Since: 0.2.0
 **/
void
gdata_contacts_contact_add_im_address (GDataContactsContact *self, GDataGDIMAddress *im_address)
{
	g_return_if_fail (GDATA_IS_CONTACTS_CONTACT (self));
	g_return_if_fail (GDATA_IS_GD_IM_ADDRESS (im_address));

	if (g_list_find_custom (self->priv->im_addresses, im_address, (GCompareFunc) gdata_gd_im_address_compare) == NULL)
		self->priv->im_addresses = g_list_append (self->priv->im_addresses, g_object_ref (im_address));
}

/**
 * gdata_contacts_contact_get_im_addresses:
 * @self: a #GDataContactsContact
 *
 * Gets a list of the IM addresses owned by the contact.
 *
 * Return value: a #GList of #GDataGDIMAddress<!-- -->es, or %NULL
 *
 * Since: 0.2.0
 **/
GList *
gdata_contacts_contact_get_im_addresses (GDataContactsContact *self)
{
	g_return_val_if_fail (GDATA_IS_CONTACTS_CONTACT (self), NULL);
	return self->priv->im_addresses;
}

/**
 * gdata_contacts_contact_get_primary_im_address:
 * @self: a #GDataContactsContact
 *
 * Gets the contact's primary IM address, if one exists.
 *
 * Return value: a #GDataGDIMAddress, or %NULL
 *
 * Since: 0.2.0
 **/
GDataGDIMAddress *
gdata_contacts_contact_get_primary_im_address (GDataContactsContact *self)
{
	GList *i;

	g_return_val_if_fail (GDATA_IS_CONTACTS_CONTACT (self), NULL);

	for (i = self->priv->im_addresses; i != NULL; i = i->next) {
		if (gdata_gd_im_address_is_primary (GDATA_GD_IM_ADDRESS (i->data)) == TRUE)
			return GDATA_GD_IM_ADDRESS (i->data);
	}

	return NULL;
}

/**
 * gdata_contacts_contact_remove_all_im_addresses:
 * @self: a #GDataContactsContact
 *
 * Removes all IM addresses from the contact.
 *
 * Since: 0.4.0
 **/
void
gdata_contacts_contact_remove_all_im_addresses (GDataContactsContact *self)
{
	GDataContactsContactPrivate *priv = self->priv;

	g_return_if_fail (GDATA_IS_CONTACTS_CONTACT (self));

	if (priv->im_addresses != NULL) {
		g_list_foreach (priv->im_addresses, (GFunc) g_object_unref, NULL);
		g_list_free (priv->im_addresses);
	}
	priv->im_addresses = NULL;
}

/**
 * gdata_contacts_contact_add_phone_number:
 * @self: a #GDataContactsContact
 * @phone_number: a #GDataGDPhoneNumber to add
 *
 * Adds a phone number to the contact's list of phone numbers and increments its reference count
 *
 * Note that only one phone number per contact may be marked as "primary". Insertion and update operations
 * (with gdata_contacts_service_insert_contact()) will return an error if more than one phone number
 * is marked as primary.
 *
 * Duplicate phone numbers will not be added to the list.
 *
 * Since: 0.2.0
 **/
void
gdata_contacts_contact_add_phone_number (GDataContactsContact *self, GDataGDPhoneNumber *phone_number)
{
	g_return_if_fail (GDATA_IS_CONTACTS_CONTACT (self));
	g_return_if_fail (GDATA_IS_GD_PHONE_NUMBER (phone_number));

	if (g_list_find_custom (self->priv->phone_numbers, phone_number, (GCompareFunc) gdata_gd_phone_number_compare) == NULL)
		self->priv->phone_numbers = g_list_append (self->priv->phone_numbers, g_object_ref (phone_number));
}

/**
 * gdata_contacts_contact_get_phone_numbers:
 * @self: a #GDataContactsContact
 *
 * Gets a list of the phone numbers owned by the contact.
 *
 * Return value: a #GList of #GDataGDPhoneNumber<!-- -->s, or %NULL
 *
 * Since: 0.2.0
 **/
GList *
gdata_contacts_contact_get_phone_numbers (GDataContactsContact *self)
{
	g_return_val_if_fail (GDATA_IS_CONTACTS_CONTACT (self), NULL);
	return self->priv->phone_numbers;
}

/**
 * gdata_contacts_contact_get_primary_phone_number:
 * @self: a #GDataContactsContact
 *
 * Gets the contact's primary phone number, if one exists.
 *
 * Return value: a #GDataGDPhoneNumber, or %NULL
 *
 * Since: 0.2.0
 **/
GDataGDPhoneNumber *
gdata_contacts_contact_get_primary_phone_number (GDataContactsContact *self)
{
	GList *i;

	g_return_val_if_fail (GDATA_IS_CONTACTS_CONTACT (self), NULL);

	for (i = self->priv->phone_numbers; i != NULL; i = i->next) {
		if (gdata_gd_phone_number_is_primary (GDATA_GD_PHONE_NUMBER (i->data)) == TRUE)
			return GDATA_GD_PHONE_NUMBER (i->data);
	}

	return NULL;
}

/**
 * gdata_contacts_contact_remove_all_phone_numbers:
 * @self: a #GDataContactsContact
 *
 * Removes all phone numbers from the contact.
 *
 * Since: 0.4.0
 **/
void
gdata_contacts_contact_remove_all_phone_numbers (GDataContactsContact *self)
{
	GDataContactsContactPrivate *priv = self->priv;

	g_return_if_fail (GDATA_IS_CONTACTS_CONTACT (self));

	if (priv->phone_numbers != NULL) {
		g_list_foreach (priv->phone_numbers, (GFunc) g_object_unref, NULL);
		g_list_free (priv->phone_numbers);
	}
	priv->phone_numbers = NULL;
}

/**
 * gdata_contacts_contact_add_postal_address:
 * @self: a #GDataContactsContact
 * @postal_address: a #GDataGDPostalAddress to add
 *
 * Adds a postal address to the contact's list of postal addresses and increments its reference count.
 *
 * Note that only one postal address per contact may be marked as "primary". Insertion and update operations
 * (with gdata_contacts_service_insert_contact()) will return an error if more than one postal address
 * is marked as primary.
 *
 * Duplicate postal addresses will not be added to the list.
 *
 * Since: 0.2.0
 **/
void
gdata_contacts_contact_add_postal_address (GDataContactsContact *self, GDataGDPostalAddress *postal_address)
{
	g_return_if_fail (GDATA_IS_CONTACTS_CONTACT (self));
	g_return_if_fail (GDATA_IS_GD_POSTAL_ADDRESS (postal_address));

	if (g_list_find_custom (self->priv->postal_addresses, postal_address, (GCompareFunc) gdata_gd_postal_address_compare) == NULL)
		self->priv->postal_addresses = g_list_append (self->priv->postal_addresses, g_object_ref (postal_address));
}

/**
 * gdata_contacts_contact_get_postal_addresses:
 * @self: a #GDataContactsContact
 *
 * Gets a list of the postal addresses owned by the contact.
 *
 * Return value: a #GList of #GDataGDPostalAddress<!-- -->es, or %NULL
 *
 * Since: 0.2.0
 **/
GList *
gdata_contacts_contact_get_postal_addresses (GDataContactsContact *self)
{
	g_return_val_if_fail (GDATA_IS_CONTACTS_CONTACT (self), NULL);
	return self->priv->postal_addresses;
}

/**
 * gdata_contacts_contact_get_primary_postal_address:
 * @self: a #GDataContactsContact
 *
 * Gets the contact's primary postal address, if one exists.
 *
 * Return value: a #GDataGDPostalAddress, or %NULL
 *
 * Since: 0.2.0
 **/
GDataGDPostalAddress *
gdata_contacts_contact_get_primary_postal_address (GDataContactsContact *self)
{
	GList *i;

	g_return_val_if_fail (GDATA_IS_CONTACTS_CONTACT (self), NULL);

	for (i = self->priv->postal_addresses; i != NULL; i = i->next) {
		if (gdata_gd_postal_address_is_primary (GDATA_GD_POSTAL_ADDRESS (i->data)) == TRUE)
			return GDATA_GD_POSTAL_ADDRESS (i->data);
	}

	return NULL;
}

/**
 * gdata_contacts_contact_remove_all_postal_addresses:
 * @self: a #GDataContactsContact
 *
 * Removes all postal addresses from the contact.
 *
 * Since: 0.4.0
 **/
void
gdata_contacts_contact_remove_all_postal_addresses (GDataContactsContact *self)
{
	GDataContactsContactPrivate *priv = self->priv;

	g_return_if_fail (GDATA_IS_CONTACTS_CONTACT (self));

	if (priv->postal_addresses != NULL) {
		g_list_foreach (priv->postal_addresses, (GFunc) g_object_unref, NULL);
		g_list_free (priv->postal_addresses);
	}
	priv->postal_addresses = NULL;
}

/**
 * gdata_contacts_contact_add_organization:
 * @self: a #GDataContactsContact
 * @organization: a #GDataGDOrganization to add
 *
 * Adds an organization to the contact's list of organizations (e.g. employers) and increments its reference count.
 *
 * Note that only one organization per contact may be marked as "primary". Insertion and update operations
 * (with gdata_contacts_service_insert_contact()) will return an error if more than one organization
 * is marked as primary.
 *
 * Duplicate organizations will not be added to the list.
 *
 * Since: 0.2.0
 **/
void
gdata_contacts_contact_add_organization (GDataContactsContact *self, GDataGDOrganization *organization)
{
	g_return_if_fail (GDATA_IS_CONTACTS_CONTACT (self));
	g_return_if_fail (organization != NULL);

	if (g_list_find_custom (self->priv->organizations, organization, (GCompareFunc) gdata_gd_organization_compare) == NULL)
		self->priv->organizations = g_list_append (self->priv->organizations, g_object_ref (organization));
}

/**
 * gdata_contacts_contact_get_organizations:
 * @self: a #GDataContactsContact
 *
 * Gets a list of the organizations to which the contact belongs.
 *
 * Return value: a #GList of #GDataGDOrganization<!-- -->s, or %NULL
 *
 * Since: 0.2.0
 **/
GList *
gdata_contacts_contact_get_organizations (GDataContactsContact *self)
{
	g_return_val_if_fail (GDATA_IS_CONTACTS_CONTACT (self), NULL);
	return self->priv->organizations;
}

/**
 * gdata_contacts_contact_get_primary_organization:
 * @self: a #GDataContactsContact
 *
 * Gets the contact's primary organization, if one exists.
 *
 * Return value: a #GDataGDOrganization, or %NULL
 *
 * Since: 0.2.0
 **/
GDataGDOrganization *
gdata_contacts_contact_get_primary_organization (GDataContactsContact *self)
{
	GList *i;

	g_return_val_if_fail (GDATA_IS_CONTACTS_CONTACT (self), NULL);

	for (i = self->priv->organizations; i != NULL; i = i->next) {
		if (gdata_gd_organization_is_primary (GDATA_GD_ORGANIZATION (i->data)) == TRUE)
			return GDATA_GD_ORGANIZATION (i->data);
	}

	return NULL;
}

/**
 * gdata_contacts_contact_remove_all_organizations:
 * @self: a #GDataContactsContact
 *
 * Removes all organizations from the contact.
 *
 * Since: 0.4.0
 **/
void
gdata_contacts_contact_remove_all_organizations (GDataContactsContact *self)
{
	GDataContactsContactPrivate *priv = self->priv;

	g_return_if_fail (GDATA_IS_CONTACTS_CONTACT (self));

	if (priv->organizations != NULL) {
		g_list_foreach (priv->organizations, (GFunc) g_object_unref, NULL);
		g_list_free (priv->organizations);
	}
	priv->organizations = NULL;
}

/**
 * gdata_contacts_contact_add_jot:
 * @self: a #GDataContactsContact
 * @jot: a #GDataGContactJot to add
 *
 * Adds a jot to the contact's list of jots and increments its reference count.
 *
 * Duplicate jots will be added to the list, and multiple jots with the same relation type can be added to a single contact.
 *
 * Since: 0.7.0
 **/
void
gdata_contacts_contact_add_jot (GDataContactsContact *self, GDataGContactJot *jot)
{
	g_return_if_fail (GDATA_IS_CONTACTS_CONTACT (self));
	g_return_if_fail (GDATA_IS_GCONTACT_JOT (jot));

	self->priv->jots = g_list_append (self->priv->jots, g_object_ref (jot));
}

/**
 * gdata_contacts_contact_get_jots:
 * @self: a #GDataContactsContact
 *
 * Gets a list of the jots attached to the contact.
 *
 * Return value: a #GList of #GDataGContactJot<!-- -->s, or %NULL
 *
 * Since: 0.7.0
 **/
GList *
gdata_contacts_contact_get_jots (GDataContactsContact *self)
{
	g_return_val_if_fail (GDATA_IS_CONTACTS_CONTACT (self), NULL);
	return self->priv->jots;
}

/**
 * gdata_contacts_contact_remove_all_jots:
 * @self: a #GDataContactsContact
 *
 * Removes all jots from the contact.
 *
 * Since: 0.7.0
 **/
void
gdata_contacts_contact_remove_all_jots (GDataContactsContact *self)
{
	GDataContactsContactPrivate *priv = self->priv;

	g_return_if_fail (GDATA_IS_CONTACTS_CONTACT (self));

	if (priv->jots != NULL) {
		g_list_foreach (priv->jots, (GFunc) g_object_unref, NULL);
		g_list_free (priv->jots);
	}
	priv->jots = NULL;
}

/**
 * gdata_contacts_contact_add_relation:
 * @self: a #GDataContactsContact
 * @relation: a #GDataGContactRelation to add
 *
 * Adds a relation to the contact's list of relations and increments its reference count.
 *
 * Duplicate relations will be added to the list, and multiple relations with the same relation type can be added to a single contact.
 * Though it may not make sense for some relation types to be repeated, adding them is allowed.
 *
 * Since: 0.7.0
 **/
void
gdata_contacts_contact_add_relation (GDataContactsContact *self, GDataGContactRelation *relation)
{
	g_return_if_fail (GDATA_IS_CONTACTS_CONTACT (self));
	g_return_if_fail (GDATA_IS_GCONTACT_RELATION (relation));

	self->priv->relations = g_list_append (self->priv->relations, g_object_ref (relation));
}

/**
 * gdata_contacts_contact_get_relations:
 * @self: a #GDataContactsContact
 *
 * Gets a list of the relations of the contact.
 *
 * Return value: a #GList of #GDataGContactRelation<!-- -->s, or %NULL
 *
 * Since: 0.7.0
 **/
GList *
gdata_contacts_contact_get_relations (GDataContactsContact *self)
{
	g_return_val_if_fail (GDATA_IS_CONTACTS_CONTACT (self), NULL);
	return self->priv->relations;
}

/**
 * gdata_contacts_contact_remove_all_relations:
 * @self: a #GDataContactsContact
 *
 * Removes all relations from the contact.
 *
 * Since: 0.7.0
 **/
void
gdata_contacts_contact_remove_all_relations (GDataContactsContact *self)
{
	GDataContactsContactPrivate *priv = self->priv;

	g_return_if_fail (GDATA_IS_CONTACTS_CONTACT (self));

	if (priv->relations != NULL) {
		g_list_foreach (priv->relations, (GFunc) g_object_unref, NULL);
		g_list_free (priv->relations);
	}
	priv->relations = NULL;
}

/**
 * gdata_contacts_contact_add_website:
 * @self: a #GDataContactsContact
 * @website: a #GDataGContactWebsite to add
 *
 * Adds a website to the contact's list of websites and increments its reference count.
 *
 * Duplicate websites will not be added to the list, though the same URI may appear in several #GDataGContactWebsite<!-- -->s with different
 * relation types or labels.
 *
 * Since: 0.7.0
 **/
void
gdata_contacts_contact_add_website (GDataContactsContact *self, GDataGContactWebsite *website)
{
	g_return_if_fail (GDATA_IS_CONTACTS_CONTACT (self));
	g_return_if_fail (GDATA_IS_GCONTACT_WEBSITE (website));

	if (g_list_find_custom (self->priv->websites, website, (GCompareFunc) gdata_gcontact_website_compare) == NULL)
		self->priv->websites = g_list_append (self->priv->websites, g_object_ref (website));
}

/**
 * gdata_contacts_contact_get_websites:
 * @self: a #GDataContactsContact
 *
 * Gets a list of the websites of the contact.
 *
 * Return value: a #GList of #GDataGContactWebsite<!-- -->s, or %NULL
 *
 * Since: 0.7.0
 **/
GList *
gdata_contacts_contact_get_websites (GDataContactsContact *self)
{
	g_return_val_if_fail (GDATA_IS_CONTACTS_CONTACT (self), NULL);
	return self->priv->websites;
}

/**
 * gdata_contacts_contact_get_primary_website:
 * @self: a #GDataContactsContact
 *
 * Gets the contact's primary website, if one exists.
 *
 * Return value: a #GDataGContactWebsite, or %NULL
 *
 * Since: 0.7.0
 **/
GDataGContactWebsite *
gdata_contacts_contact_get_primary_website (GDataContactsContact *self)
{
	GList *i;

	g_return_val_if_fail (GDATA_IS_CONTACTS_CONTACT (self), NULL);

	for (i = self->priv->websites; i != NULL; i = i->next) {
		if (gdata_gcontact_website_is_primary (GDATA_GCONTACT_WEBSITE (i->data)) == TRUE)
			return GDATA_GCONTACT_WEBSITE (i->data);
	}

	return NULL;
}

/**
 * gdata_contacts_contact_remove_all_websites:
 * @self: a #GDataContactsContact
 *
 * Removes all websites from the contact.
 *
 * Since: 0.7.0
 **/
void
gdata_contacts_contact_remove_all_websites (GDataContactsContact *self)
{
	GDataContactsContactPrivate *priv = self->priv;

	g_return_if_fail (GDATA_IS_CONTACTS_CONTACT (self));

	if (priv->websites != NULL) {
		g_list_foreach (priv->websites, (GFunc) g_object_unref, NULL);
		g_list_free (priv->websites);
	}
	priv->websites = NULL;
}

/**
 * gdata_contacts_contact_add_event:
 * @self: a #GDataContactsContact
 * @event: a #GDataGContactEvent to add
 *
 * Adds an event to the contact's list of events and increments its reference count.
 *
 * Duplicate events will be added to the list, and multiple events with the same event type can be added to a single contact.
 * Though it may not make sense for some event types to be repeated, adding them is allowed.
 *
 * Since: 0.7.0
 **/
void
gdata_contacts_contact_add_event (GDataContactsContact *self, GDataGContactEvent *event)
{
	g_return_if_fail (GDATA_IS_CONTACTS_CONTACT (self));
	g_return_if_fail (GDATA_IS_GCONTACT_EVENT (event));

	self->priv->events = g_list_append (self->priv->events, g_object_ref (event));
}

/**
 * gdata_contacts_contact_get_events:
 * @self: a #GDataContactsContact
 *
 * Gets a list of the events of the contact.
 *
 * Return value: a #GList of #GDataGContactEvent<!-- -->s, or %NULL
 *
 * Since: 0.7.0
 **/
GList *
gdata_contacts_contact_get_events (GDataContactsContact *self)
{
	g_return_val_if_fail (GDATA_IS_CONTACTS_CONTACT (self), NULL);
	return self->priv->events;
}

/**
 * gdata_contacts_contact_remove_all_events:
 * @self: a #GDataContactsContact
 *
 * Removes all events from the contact.
 *
 * Since: 0.7.0
 **/
void
gdata_contacts_contact_remove_all_events (GDataContactsContact *self)
{
	GDataContactsContactPrivate *priv = self->priv;

	g_return_if_fail (GDATA_IS_CONTACTS_CONTACT (self));

	if (priv->events != NULL) {
		g_list_foreach (priv->events, (GFunc) g_object_unref, NULL);
		g_list_free (priv->events);
	}
	priv->events = NULL;
}

/**
 * gdata_contacts_contact_add_calendar:
 * @self: a #GDataContactsContact
 * @calendar: a #GDataGContactCalendar to add
 *
 * Adds a calendar to the contact's list of calendars and increments its reference count.
 *
 * Duplicate calendars will not be added to the list, though the same URI may appear in several #GDataGContactCalendar<!-- -->s with different
 * relation types or labels.
 *
 * Since: 0.7.0
 **/
void
gdata_contacts_contact_add_calendar (GDataContactsContact *self, GDataGContactCalendar *calendar)
{
	g_return_if_fail (GDATA_IS_CONTACTS_CONTACT (self));
	g_return_if_fail (GDATA_IS_GCONTACT_CALENDAR (calendar));

	if (g_list_find_custom (self->priv->calendars, calendar, (GCompareFunc) gdata_gcontact_calendar_compare) == NULL)
		self->priv->calendars = g_list_append (self->priv->calendars, g_object_ref (calendar));
}

/**
 * gdata_contacts_contact_get_calendars:
 * @self: a #GDataContactsContact
 *
 * Gets a list of the calendars of the contact.
 *
 * Return value: a #GList of #GDataGContactCalendar<!-- -->s, or %NULL
 *
 * Since: 0.7.0
 **/
GList *
gdata_contacts_contact_get_calendars (GDataContactsContact *self)
{
	g_return_val_if_fail (GDATA_IS_CONTACTS_CONTACT (self), NULL);
	return self->priv->calendars;
}

/**
 * gdata_contacts_contact_get_primary_calendar:
 * @self: a #GDataContactsContact
 *
 * Gets the contact's primary calendar, if one exists.
 *
 * Return value: a #GDataGContactCalendar, or %NULL
 *
 * Since: 0.7.0
 **/
GDataGContactCalendar *
gdata_contacts_contact_get_primary_calendar (GDataContactsContact *self)
{
	GList *i;

	g_return_val_if_fail (GDATA_IS_CONTACTS_CONTACT (self), NULL);

	for (i = self->priv->calendars; i != NULL; i = i->next) {
		if (gdata_gcontact_calendar_is_primary (GDATA_GCONTACT_CALENDAR (i->data)) == TRUE)
			return GDATA_GCONTACT_CALENDAR (i->data);
	}

	return NULL;
}

/**
 * gdata_contacts_contact_remove_all_calendars:
 * @self: a #GDataContactsContact
 *
 * Removes all calendars from the contact.
 *
 * Since: 0.7.0
 **/
void
gdata_contacts_contact_remove_all_calendars (GDataContactsContact *self)
{
	GDataContactsContactPrivate *priv = self->priv;

	g_return_if_fail (GDATA_IS_CONTACTS_CONTACT (self));

	if (priv->calendars != NULL) {
		g_list_foreach (priv->calendars, (GFunc) g_object_unref, NULL);
		g_list_free (priv->calendars);
	}
	priv->calendars = NULL;
}

/**
 * gdata_contacts_contact_get_extended_property:
 * @self: a #GDataContactsContact
 * @name: the property name; an arbitrary, unique string
 *
 * Gets the value of an extended property of the contact. Each contact can have up to 10 client-set extended
 * properties to store data of the client's choosing.
 *
 * Return value: the property's value, or %NULL
 *
 * Since: 0.2.0
 **/
const gchar *
gdata_contacts_contact_get_extended_property (GDataContactsContact *self, const gchar *name)
{
	g_return_val_if_fail (GDATA_IS_CONTACTS_CONTACT (self), NULL);
	g_return_val_if_fail (name != NULL, NULL);
	return g_hash_table_lookup (self->priv->extended_properties, name);
}

/**
 * gdata_contacts_contact_get_extended_properties:
 * @self: a #GDataContactsContact
 *
 * Gets the full list of extended properties of the contact; a hash table mapping property name to value.
 *
 * Return value: a #GHashTable of extended properties
 *
 * Since: 0.4.0
 **/
GHashTable *
gdata_contacts_contact_get_extended_properties (GDataContactsContact *self)
{
	g_return_val_if_fail (GDATA_IS_CONTACTS_CONTACT (self), NULL);
	return self->priv->extended_properties;
}

/**
 * gdata_contacts_contact_set_extended_property:
 * @self: a #GDataContactsContact
 * @name: the property name; an arbitrary, unique string
 * @value: the property value, or %NULL
 *
 * Sets the value of a contact's extended property. Extended property names are unique (but of the client's choosing),
 * and reusing the same property name will result in the old value of that property being overwritten.
 *
 * To unset a property, set @value to %NULL.
 *
 * A contact may have up to 10 extended properties, and each should be reasonably small (i.e. not a photo or ringtone).
 * For more information, see the <ulink type="http"
 * url="http://code.google.com/apis/contacts/docs/2.0/reference.html#ProjectionsAndExtended">online documentation</ulink>.
 * %FALSE will be returned if you attempt to add more than 10 extended properties.
 *
 * Return value: %TRUE if the property was updated or deleted successfully, %FALSE otherwise
 *
 * Since: 0.2.0
 **/
gboolean
gdata_contacts_contact_set_extended_property (GDataContactsContact *self, const gchar *name, const gchar *value)
{
	GHashTable *extended_properties = self->priv->extended_properties;

	g_return_val_if_fail (GDATA_IS_CONTACTS_CONTACT (self), FALSE);
	g_return_val_if_fail (name != NULL, FALSE);

	if (value == NULL || *value == '\0') {
		/* Removing a property */
		g_hash_table_remove (extended_properties, name);
		return TRUE;
	}

	/* We can't add more than MAX_N_EXTENDED_PROPERTIES */
	if (g_hash_table_lookup (extended_properties, name) == NULL &&
	    g_hash_table_size (extended_properties) >= MAX_N_EXTENDED_PROPERTIES)
		return FALSE;

	/* Updating an existing property or adding a new one */
	g_hash_table_insert (extended_properties, g_strdup (name), g_strdup (value));

	return TRUE;
}

/**
 * gdata_contacts_contact_add_group:
 * @self: a #GDataContactsContact
 * @href: the group's ID URI
 *
 * Adds the contact to the given group. @href should be a URI.
 *
 * Since: 0.2.0
 **/
void
gdata_contacts_contact_add_group (GDataContactsContact *self, const gchar *href)
{
	g_return_if_fail (GDATA_IS_CONTACTS_CONTACT (self));
	g_return_if_fail (href != NULL);
	g_hash_table_insert (self->priv->groups, (gchar*) href, GUINT_TO_POINTER (FALSE));
}

/**
 * gdata_contacts_contact_remove_group:
 * @self: a #GDataContactsContact
 * @href: the group's ID URI
 *
 * Removes the contact from the given group. @href should be a URI.
 *
 * Since: 0.2.0
 **/
void
gdata_contacts_contact_remove_group (GDataContactsContact *self, const gchar *href)
{
	g_return_if_fail (GDATA_IS_CONTACTS_CONTACT (self));
	g_return_if_fail (href != NULL);
	g_hash_table_remove (self->priv->groups, href);
}

/**
 * gdata_contacts_contact_is_group_deleted:
 * @self: a #GDataContactsContact
 * @href: the group's ID URI
 *
 * Returns whether the contact has recently been removed from the given group. This
 * will always return %FALSE unless #GDataContactsQuery:show-deleted has been set to
 * %TRUE for the query which returned the contact.
 *
 * Return value: %TRUE if the contact has recently been removed from the group, %FALSE otherwise
 *
 * Since: 0.2.0
 **/
gboolean
gdata_contacts_contact_is_group_deleted (GDataContactsContact *self, const gchar *href)
{
	g_return_val_if_fail (GDATA_IS_CONTACTS_CONTACT (self), FALSE);
	g_return_val_if_fail (href != NULL, FALSE);
	return GPOINTER_TO_UINT (g_hash_table_lookup (self->priv->groups, href));
}

static void
get_groups_cb (const gchar *href, gpointer deleted, GList **groups)
{
	*groups = g_list_prepend (*groups, (gchar*) href);
}

/**
 * gdata_contacts_contact_get_groups:
 * @self: a #GDataContactsContact
 *
 * Gets a list of the groups to which the contact belongs.
 *
 * Return value: a #GList of constant group ID URIs, or %NULL; free with g_list_free()
 *
 * Since: 0.2.0
 **/
GList *
gdata_contacts_contact_get_groups (GDataContactsContact *self)
{
	GList *groups;

	g_return_val_if_fail (GDATA_IS_CONTACTS_CONTACT (self), NULL);

	g_hash_table_foreach (self->priv->groups, (GHFunc) get_groups_cb, &groups);
	return g_list_reverse (groups);
}

/**
 * gdata_contacts_contact_is_deleted:
 * @self: a #GDataContactsContact
 *
 * Returns whether the contact has recently been deleted. This will always return
 * %FALSE unless #GDataContactsQuery:show-deleted has been set to
 * %TRUE for the query which returned the contact; then this function will return
 * %TRUE only if the contact has been deleted.
 *
 * If a contact has been deleted, no other information is available about it. This
 * is designed to allow contacts to be deleted from local address books using
 * incremental updates from the server (e.g. with #GDataQuery:updated-min and
 * #GDataContactsQuery:show-deleted).
 *
 * Return value: %TRUE if the contact has been deleted, %FALSE otherwise
 *
 * Since: 0.2.0
 **/
gboolean
gdata_contacts_contact_is_deleted (GDataContactsContact *self)
{
	g_return_val_if_fail (GDATA_IS_CONTACTS_CONTACT (self), FALSE);
	return self->priv->deleted;
}

/**
 * gdata_contacts_contact_has_photo:
 * @self: a #GDataContactsContact
 *
 * Returns whether the contact has a photo attached to their contact entry. If the contact
 * does have a photo, it can be returned using gdata_contacts_contact_get_photo().
 *
 * Return value: %TRUE if the contact has a photo, %FALSE otherwise
 *
 * Since: 0.4.0
 **/
gboolean
gdata_contacts_contact_has_photo (GDataContactsContact *self)
{
	g_return_val_if_fail (GDATA_IS_CONTACTS_CONTACT (self), FALSE);
	return (self->priv->photo_etag != NULL) ? TRUE : FALSE;
}

/**
 * gdata_contacts_contact_get_photo:
 * @self: a #GDataContactsContact
 * @service: a #GDataContactsService
 * @length: return location for the image length, in bytes
 * @content_type: return location for the image's content type, or %NULL; free with g_free()
 * @cancellable: optional #GCancellable object, or %NULL
 * @error: a #GError, or %NULL
 *
 * Downloads and returns the contact's photo, if they have one. If the contact doesn't
 * have a photo (i.e. gdata_contacts_contact_has_photo() returns %FALSE), %NULL is returned, but
 * no error is set in @error.
 *
 * If @cancellable is not %NULL, then the operation can be cancelled by triggering the @cancellable object from another thread.
 * If the operation was cancelled, the error %G_IO_ERROR_CANCELLED will be returned.
 *
 * If there is an error getting the photo, a %GDATA_SERVICE_ERROR_PROTOCOL_ERROR error will be returned.
 *
 * Return value: the image data, or %NULL; free with g_free()
 *
 * Since: 0.4.0
 **/
gchar *
gdata_contacts_contact_get_photo (GDataContactsContact *self, GDataContactsService *service, gsize *length, gchar **content_type,
				  GCancellable *cancellable, GError **error)
{
	GDataServiceClass *klass;
	GDataLink *link;
	SoupMessage *message;
	guint status;
	gchar *data;

	/* TODO: async version */
	g_return_val_if_fail (GDATA_IS_CONTACTS_CONTACT (self), NULL);
	g_return_val_if_fail (GDATA_IS_CONTACTS_SERVICE (service), NULL);
	g_return_val_if_fail (length != NULL, NULL);

	/* Return if there is no photo */
	if (gdata_contacts_contact_has_photo (self) == FALSE)
		return NULL;

	/* Get the photo URI */
	link = gdata_entry_look_up_link (GDATA_ENTRY (self), "http://schemas.google.com/contacts/2008/rel#photo");
	g_assert (link != NULL);
	message = soup_message_new (SOUP_METHOD_GET, gdata_link_get_uri (link));

	/* Make sure the headers are set */
	klass = GDATA_SERVICE_GET_CLASS (service);
	if (klass->append_query_headers != NULL)
		klass->append_query_headers (GDATA_SERVICE (service), message);

	/* Send the message */
	status = _gdata_service_send_message (GDATA_SERVICE (service), message, error);
	if (status == SOUP_STATUS_NONE) {
		g_object_unref (message);
		return NULL;
	}

	/* Check for cancellation */
	if (g_cancellable_set_error_if_cancelled (cancellable, error) == TRUE) {
		g_object_unref (message);
		return NULL;
	}

	if (status != SOUP_STATUS_OK) {
		/* Error */
		g_assert (klass->parse_error_response != NULL);
		klass->parse_error_response (GDATA_SERVICE (service), GDATA_OPERATION_DOWNLOAD, status, message->reason_phrase,
		                             message->response_body->data, message->response_body->length, error);
		g_object_unref (message);
		return NULL;
	}

	g_assert (message->response_body->data != NULL);

	/* Sort out the return values */
	if (content_type != NULL)
		*content_type = g_strdup (soup_message_headers_get_content_type (message->response_headers, NULL));
	*length = message->response_body->length;
	data = g_memdup (message->response_body->data, message->response_body->length);

	/* Update the stored photo ETag */
	g_free (self->priv->photo_etag);
	self->priv->photo_etag = g_strdup (soup_message_headers_get_one (message->response_headers, "ETag"));
	g_object_unref (message);

	return data;
}

/**
 * gdata_contacts_contact_set_photo:
 * @self: a #GDataContactsContact
 * @service: a #GDataService
 * @data: the image data, or %NULL
 * @length: the image length, in bytes, or <code class="literal">0</code>
 * @cancellable: optional #GCancellable object, or %NULL
 * @error: a #GError, or %NULL
 *
 * Sets the contact's photo to @data or, if @data is %NULL, deletes the contact's photo.
 *
 * If @cancellable is not %NULL, then the operation can be cancelled by triggering the @cancellable object from another thread.
 * If the operation was cancelled, the error %G_IO_ERROR_CANCELLED will be returned.
 *
 * If there is an error setting the photo, a %GDATA_SERVICE_ERROR_PROTOCOL_ERROR error will be returned.
 *
 * Return value: %TRUE on success, %FALSE otherwise
 *
 * Since: 0.4.0
 **/
gboolean
gdata_contacts_contact_set_photo (GDataContactsContact *self, GDataService *service, const gchar *data, gsize length,
				  GCancellable *cancellable, GError **error)
{
	GDataServiceClass *klass;
	GDataLink *link;
	SoupMessage *message;
	guint status;
	gboolean adding_photo = FALSE, deleting_photo = FALSE;

	/* TODO: async version */
	g_return_val_if_fail (GDATA_IS_CONTACTS_CONTACT (self), FALSE);
	g_return_val_if_fail (GDATA_IS_SERVICE (service), FALSE);

	if (self->priv->photo_etag == NULL && data != NULL)
		adding_photo = TRUE;
	else if (self->priv->photo_etag != NULL && data == NULL)
		deleting_photo = TRUE;

	/* Get the photo URI */
	link = gdata_entry_look_up_link (GDATA_ENTRY (self), "http://schemas.google.com/contacts/2008/rel#photo");
	g_assert (link != NULL);
	if (deleting_photo == TRUE)
		message = soup_message_new (SOUP_METHOD_DELETE, gdata_link_get_uri (link));
	else
		message = soup_message_new (SOUP_METHOD_PUT, gdata_link_get_uri (link));

	/* Make sure the headers are set */
	klass = GDATA_SERVICE_GET_CLASS (service);
	if (klass->append_query_headers != NULL)
		klass->append_query_headers (service, message);

	/* Append the ETag header if possible */
	if (self->priv->photo_etag != NULL)
		soup_message_headers_append (message->request_headers, "If-Match", self->priv->photo_etag);

	if (deleting_photo == FALSE) {
		/* Append the data */
		soup_message_set_request (message, "image/*", SOUP_MEMORY_STATIC, (gchar*) data, length);
	}

	/* Send the message */
	status = _gdata_service_send_message (service, message, error);
	if (status == SOUP_STATUS_NONE) {
		g_object_unref (message);
		return FALSE;
	}

	/* Check for cancellation */
	if (g_cancellable_set_error_if_cancelled (cancellable, error) == TRUE) {
		g_object_unref (message);
		return FALSE;
	}

	if (status != SOUP_STATUS_OK) {
		/* Error */
		g_assert (klass->parse_error_response != NULL);
		klass->parse_error_response (service, GDATA_OPERATION_UPLOAD, status, message->reason_phrase, message->response_body->data,
		                             message->response_body->length, error);
		g_object_unref (message);
		return FALSE;
	}

	/* Update the stored photo ETag */
	g_free (self->priv->photo_etag);
	self->priv->photo_etag = g_strdup (soup_message_headers_get_one (message->response_headers, "ETag"));
	g_object_unref (message);

	if (adding_photo == TRUE || deleting_photo == TRUE)
		g_object_notify (G_OBJECT (self), "has-photo");

	return TRUE;
}
