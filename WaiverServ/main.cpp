#include <gtk/gtk.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>


//Colors
GdkRGBA gdk_unfilled;
GdkRGBA gdk_confirm;
GdkRGBA gdk_trans = { 0, 0, 0, 0 };

//Widget Definitions;

GtkWidget* window;
GdkScreen* screen;

GdkPixbuf* rawWaiverPixBuf;

GtkWidget* GlobalBox;

GtkWidget* mainSigClickLabel;
GtkWidget* mainSigClickSignBox;

GdkPixbuf* mainSigPixBufImg;
GtkWidget* mainSigImg;
GtkWidget* mainSigImgBox;

GtkWidget* mainSigNameLabel;
GtkWidget* mainSigNameLabelBox;

GtkWidget* MainSigNameEntry;
GtkWidget* MainSigNameEntryBox;


GtkWidget* parentSigClickLabel;
GtkWidget* parentSigClickSignBox;

GdkPixbuf* parentSigPixBufImg;
GtkWidget* parentSigImg;
GtkWidget* parentSigImgBox;

GtkWidget* parentSigNameLabel;
GtkWidget* parentSigNameLabelBox;

GtkWidget* ParentSigNameEntry;
GtkWidget* ParentSigNameEntryBox;

GtkWidget* mainDate;
GtkWidget* parentDate;

GtkWidget* isMinorLabel;
GtkWidget* isMinorBox;

GtkWidget* statusLabel;

int tp;
bool sigMainActive;
bool sigParentActive;

bool validMainSig = false;
bool validParentSig = false;;

bool isMinor = true;


bool penDown = false;

int lastScaledX = -1;
int lastScaledY = -1;

int MessageDelay = 0;

bool SubmitConfirmed = false;

void ResetScreen()
{
	gtk_widget_show(GTK_WIDGET(mainSigClickSignBox));
	gtk_widget_show(GTK_WIDGET(parentSigClickSignBox));


	gtk_label_set_text(GTK_LABEL(mainSigNameLabel), "CLICK HERE TO ENTER NAME");
	gtk_widget_override_background_color(GTK_WIDGET(mainSigNameLabel), GTK_STATE_FLAG_NORMAL, &gdk_unfilled);


	gtk_label_set_text(GTK_LABEL(parentSigNameLabel), "CLICK HERE TO ENTER NAME");
	gtk_widget_override_background_color(GTK_WIDGET(parentSigNameLabel), GTK_STATE_FLAG_NORMAL, &gdk_unfilled);
	gtk_widget_show(GTK_WIDGET(parentSigNameLabelBox));






	gtk_widget_show(GTK_WIDGET(isMinorLabel));
	gtk_label_set_text(GTK_LABEL(isMinorLabel), "CLICK HERE IF OVER 18");
	gtk_widget_override_background_color(GTK_WIDGET(isMinorLabel), GTK_STATE_FLAG_NORMAL, &gdk_unfilled);


	//Hide Inputs To Start
	gtk_widget_hide(GTK_WIDGET(mainSigImgBox));
	gtk_widget_hide(GTK_WIDGET(MainSigNameEntry));
	gtk_widget_hide(GTK_WIDGET(parentSigImgBox));
	gtk_widget_hide(GTK_WIDGET(ParentSigNameEntry));


	isMinor = true;
	validMainSig = false;
	validParentSig = false;;

	return;


}
gint message_handler(gpointer data)
{

	if ((MessageDelay == 3) && SubmitConfirmed)
	{
		//Make Directory If doesn't Exist
		GdkWindow* gdk_window = gtk_widget_get_window(GTK_WIDGET(window));
		GdkPixbuf* screenshot = gdk_pixbuf_get_from_window(GDK_WINDOW(gdk_window), 0, 0, gdk_pixbuf_get_width(rawWaiverPixBuf), gdk_pixbuf_get_height(rawWaiverPixBuf));

		char fPath[256];

		time_t now = time(0);
		tm* ltm = localtime(&now);

		char datestring[128];
		strftime(datestring, sizeof(datestring), "%d%m%C_%H%M%S", ltm);

		sprintf(fPath, "/FSWaivers/FSwaiver_%s.png", datestring);
		gdk_pixbuf_save(screenshot, fPath, "png", NULL, NULL);
	}



	if (MessageDelay < 8)
		MessageDelay++;
	else
	{
		gtk_widget_hide(GTK_WIDGET(statusLabel));
		if (SubmitConfirmed)
		{
			ResetScreen();
			SubmitConfirmed = false;
		}
	}
	return 1;
}

gint date_handler(gpointer data)
{
	//Date Labels
	time_t now = time(0);
	tm* ltm = localtime(&now);

	char datestring[128];

	strftime(datestring, sizeof(datestring), "%B %d, %Y", ltm);

	gtk_label_set_text(GTK_LABEL(mainDate), datestring);
	gtk_label_set_text(GTK_LABEL(parentDate), datestring);

	return 1;
	
}

gint touchpad_handler(gpointer data)
{
	//Open touchpad if it is not open
	if (tp == 0)
	{
		tp = open("/dev/topaz", O_RDONLY);

		//Check if open error. May Not have Existed?
		if (tp < 0)
		{
			tp = 0;
			return 1;
		}
		//Set to Non Blocking
		int flags = fcntl(tp, F_GETFL, 0);
		if (fcntl(tp, F_SETFL, flags | O_NONBLOCK) < 0)
		{
			close(tp);
			return 1;
		}
	}


	while (1)
	{
	
		//Spit out all relevant data
		//Packet Sieze is 8 bytes
		unsigned char tpPacket[8] = { 0 };
		ssize_t len = read(tp, tpPacket, sizeof(tpPacket));
		//No data to read
		if (len < 0)
			break;


		//Process Data if Sig is active
		if (sigMainActive || sigParentActive)
		{
			//Print Out data to debug
			
			short X = tpPacket[2] + tpPacket[3] * 127;
			short Y = tpPacket[4] + tpPacket[5] * 127;

			int scaledX = (X - 500) / 6;
			int scaledY = ((Y - 350) /6);

			//g_print("%d %d\n", scaledX, scaledY);

			//g_print("X: %d; Y: %d\n", X, Y);

			//g_print("%02X %02X   %02X %02X  %02X %02X   %02X %02X     %d %d\n", tpPacket[0],  tpPacket[1],  tpPacket[2],  tpPacket[3], tpPacket[4], tpPacket[5], tpPacket[6], tpPacket[7], X, Y);


			//Draw Lines;
			if (tpPacket[1] == 0x80)
			{
				lastScaledX = -1;
				lastScaledY = -1;
				continue;
			}

			

			//Get PixBuf Pixels
			guchar* pPixels;
			if (sigMainActive )
				pPixels = gdk_pixbuf_get_pixels(mainSigPixBufImg);
			else
				pPixels = gdk_pixbuf_get_pixels(parentSigPixBufImg);

			//Continue of out of range
			if (!(scaledX >= 0 && scaledY >= 0 && scaledX < gdk_pixbuf_get_width(mainSigPixBufImg) && scaledY < gdk_pixbuf_get_height(mainSigPixBufImg)))
			{
				lastScaledX = -1;
				lastScaledY = -1;
				continue;
			}
			//Do we have point to interpolate from?
			if ((lastScaledX == -1) || (lastScaledY == -1))
			{
				lastScaledX = scaledX;
				lastScaledY = scaledY;
				continue;
			}
			
			//Interpolate between old and new
			if (abs(scaledX - lastScaledX) > abs(scaledY - lastScaledY))
			//Interpolate using X indexes
			{
				for (int iX = scaledX; iX != lastScaledX; (scaledX < lastScaledX) ? iX++ : iX--)
				{
					if (lastScaledX == scaledX)
						continue;

					int iY = scaledY + (iX - scaledX) * (lastScaledY - scaledY) / (lastScaledX - scaledX);

					int offset = iY * gdk_pixbuf_get_rowstride(mainSigPixBufImg) + iX * gdk_pixbuf_get_n_channels(mainSigPixBufImg);
					guchar* pixel = &pPixels[offset]; // get pixel pointer
					pixel[0] = 0x00; pixel[1] = 0x00; pixel[2] = 0x00; pixel[3] = 0xFF;

				}

			}
			else
			//Interpolate using Y indexes
			{
				for (int iY = scaledY; iY != lastScaledY; (scaledY < lastScaledY) ? iY++ : iY--)
				{
					if (lastScaledY == scaledY)
						continue;
					
					int iX = scaledX + (iY - scaledY) * (lastScaledX - scaledX) / (lastScaledY - scaledY);

					int offset = iY * gdk_pixbuf_get_rowstride(mainSigPixBufImg) + iX * gdk_pixbuf_get_n_channels(mainSigPixBufImg);
					guchar * pixel = &pPixels[offset]; // get pixel pointer
					pixel[0] = 0x00; pixel[1] = 0x00; pixel[2] = 0x00; pixel[3] = 0xFF;
				}
			}

			//Record old values
			lastScaledX = scaledX;
			lastScaledY = scaledY;
		}

		if (sigMainActive)
			gtk_image_set_from_pixbuf(GTK_IMAGE(mainSigImg), mainSigPixBufImg);
		if(sigParentActive)
			gtk_image_set_from_pixbuf(GTK_IMAGE(parentSigImg), parentSigPixBufImg);
	}

	return 1;
}



gboolean mainCleanup(GtkWidget* widget, GdkEvent* event, gpointer user_data)
{
	//Close Touchpad File
	close(tp);

}



void finalizeActiveFields()
{
	//Clear out Main Sig Field
	if(sigMainActive)
	{
		sigMainActive = false;
		int sigPoints = 0;
		guchar* pPixels = gdk_pixbuf_get_pixels(mainSigPixBufImg);
		for (int X = 0; X < gdk_pixbuf_get_width(mainSigPixBufImg); X++)
		{
			for (int Y = 0; Y < gdk_pixbuf_get_height(mainSigPixBufImg); Y++)
			{
				int offset = Y * gdk_pixbuf_get_rowstride(mainSigPixBufImg) + X * gdk_pixbuf_get_n_channels(mainSigPixBufImg);
				guchar* pixel = &pPixels[offset]; // get pixel pointer
				//Make all non balck pixels transparent
				if (!((pixel[0] == 0x00) && (pixel[1] == 0x00) && (pixel[2] == 0x00) && (pixel[3] = 0xFF)))
				{
					pixel[0] = 0x00; pixel[1] = 0x00; pixel[2] = 0x00; pixel[3] = 0x00;
				}
				else
				{
					sigPoints++;
				}

			}
		}

		gtk_image_set_from_pixbuf(GTK_IMAGE(mainSigImg), mainSigPixBufImg);

		//Check if there are few points
		if (sigPoints < 30)
		{
			//Show Label Hide Box
			gtk_widget_show(mainSigClickSignBox);
			gtk_widget_hide(mainSigImgBox);
			validMainSig = false;
		}
		else
			validMainSig = true;
	}


	//Check if already filled out

	//Sig Name Entry Finalize
	
	const gchar* MainSigNameText = gtk_entry_get_text(GTK_ENTRY(MainSigNameEntry));
	const gchar* MainSigNameEnteredText = gtk_label_get_text(GTK_LABEL(mainSigNameLabel));

	if (strlen(MainSigNameText) > 0)
	{
		gtk_label_set_text(GTK_LABEL(mainSigNameLabel), MainSigNameText);
		gtk_widget_override_background_color(GTK_WIDGET(mainSigNameLabel), GTK_STATE_FLAG_NORMAL, &gdk_trans);
	}
	else
	{
		//Don't erase existing data
		if(strlen(MainSigNameEnteredText) <= 0)
		{
			gtk_label_set_text(GTK_LABEL(mainSigNameLabel), "CLICK HERE TO ENTER NAME");
			gtk_widget_override_background_color(GTK_WIDGET(mainSigNameLabel), GTK_STATE_FLAG_NORMAL, &gdk_unfilled);
		}
	}

	gtk_entry_set_text(GTK_ENTRY(MainSigNameEntry), "");

	gtk_widget_show(GTK_WIDGET(mainSigNameLabelBox));
	gtk_widget_hide(GTK_WIDGET(MainSigNameEntry));
	



	if (sigParentActive)
	{
		sigParentActive = false;
		//Clear out Parent Sig Field
		sigParentActive = false;
		int sigPoints = 0;
		guchar* pPixels = gdk_pixbuf_get_pixels(parentSigPixBufImg);
		for (int X = 0; X < gdk_pixbuf_get_width(parentSigPixBufImg); X++)
		{
			for (int Y = 0; Y < gdk_pixbuf_get_height(parentSigPixBufImg); Y++)
			{
				int offset = Y * gdk_pixbuf_get_rowstride(parentSigPixBufImg) + X * gdk_pixbuf_get_n_channels(parentSigPixBufImg);
				guchar* pixel = &pPixels[offset]; // get pixel pointer
				//Make all non balck pixels transparent
				if (!((pixel[0] == 0x00) && (pixel[1] == 0x00) && (pixel[2] == 0x00) && (pixel[3] = 0xFF)))
				{
					pixel[0] = 0x00; pixel[1] = 0x00; pixel[2] = 0x00; pixel[3] = 0x00;
				}
				else
				{
					sigPoints++;
				}

			}
		}

		gtk_image_set_from_pixbuf(GTK_IMAGE(parentSigImg), parentSigPixBufImg);
		

		//Check if there are few points
		if (sigPoints < 30)
		{
			//Show Label Hide Box
			gtk_widget_show(parentSigClickSignBox);
			gtk_widget_hide(parentSigImgBox);
			validParentSig = false;
		}
		else {
			validParentSig = true;
			isMinor = true;
			gtk_label_set_text(GTK_LABEL(isMinorLabel), "CLICK HERE IF OVER 18");
			gtk_widget_override_background_color(GTK_WIDGET(isMinorLabel), GTK_STATE_FLAG_NORMAL, &gdk_confirm);
			
		}
	}

	

	//ParentSig Name Entry Finalize
	
	const gchar* ParentSigNameText = gtk_entry_get_text(GTK_ENTRY(ParentSigNameEntry));

	//If we have a name to update
	if (strlen(ParentSigNameText) > 0)
	{
		//Update displayed Name
		gtk_label_set_text(GTK_LABEL(parentSigNameLabel), ParentSigNameText);
		gtk_widget_override_background_color(GTK_WIDGET(parentSigNameLabel), GTK_STATE_FLAG_NORMAL, &gdk_trans);

		//Show The Label
		gtk_widget_show(GTK_WIDGET(parentSigNameLabelBox));
		gtk_widget_hide(GTK_WIDGET(ParentSigNameEntry));

		//Reset Entry Text
		gtk_entry_set_text(GTK_ENTRY(ParentSigNameEntry), "");

		//Clear Out The Minor Box
		isMinor = true;
		gtk_label_set_text(GTK_LABEL(isMinorLabel), "CLICK HERE IF OVER 18");
		gtk_widget_override_background_color(GTK_WIDGET(isMinorLabel), GTK_STATE_FLAG_NORMAL, &gdk_confirm);
	}

}


gboolean SubmitClicked(GtkButton* button, gpointer user_data)
{
	finalizeActiveFields();
	
	bool ValidData = true;


	if (!validMainSig)
		ValidData = false;


	//Name Text
	const gchar* MainSigNameEnteredText = gtk_label_get_text(GTK_LABEL(mainSigNameLabel));
	if (strcmp(MainSigNameEnteredText, "CLICK HERE TO ENTER NAME") == 0)
		ValidData = false;





	if (isMinor)  //For Minors Second Sig Needed
	{
		if (!validParentSig)
			ValidData = false;

		//Name Text
		const gchar* ParentSigNameEnteredText = gtk_label_get_text(GTK_LABEL(parentSigNameLabel));
		if (strcmp(ParentSigNameEnteredText, "CLICK HERE TO ENTER NAME") == 0)
			ValidData = false;
	}




	if (ValidData)
	{

		if (SubmitConfirmed)
			return true;

		SubmitConfirmed = true;
		MessageDelay = 0;

		gtk_label_set_text(GTK_LABEL(statusLabel), "Thank You For Your Submission.");
		gtk_widget_override_background_color(GTK_WIDGET(statusLabel), GTK_STATE_FLAG_NORMAL, &gdk_confirm);


		gtk_widget_show(GTK_WIDGET(statusLabel));
		gtk_widget_hide(GTK_WIDGET(isMinorLabel));

				
		


	}
	else
	{
		gtk_label_set_text(GTK_LABEL(statusLabel), "Error: Please Fill Out Red Fields.");
		gtk_widget_override_background_color(GTK_WIDGET(statusLabel), GTK_STATE_FLAG_NORMAL, &gdk_unfilled);

		gtk_widget_show(GTK_WIDGET(statusLabel));
		MessageDelay = 0;
	}





	return true;
}

gboolean CancelClicked(GtkButton * button, gpointer user_data)
{
	ResetScreen();
	return true;
}


gboolean GlobalKeyPress(GtkWidget* widget, GdkEvent* event, gpointer  user_data)
{
	if ((((GdkEventKey*)event)->keyval == GDK_KEY_Return) || (((GdkEventKey*)event)->keyval == GDK_KEY_Tab))
	{
		finalizeActiveFields();
		return true;
	}
	return false;
}



gboolean isMinorClick(GtkWidget* widget, GdkEvent* event, gpointer user_data)
{
	finalizeActiveFields();

	if (isMinor)
	{
		isMinor = false;
		gtk_label_set_text(GTK_LABEL(isMinorLabel), "CLICK HERE IF UNDER 18");
		gtk_label_set_text(GTK_LABEL(parentSigNameLabel), "");
		gtk_widget_hide(GTK_WIDGET(parentSigClickSignBox));
		gtk_widget_hide(GTK_WIDGET(parentSigImgBox));
		gtk_widget_hide(GTK_WIDGET(parentSigNameLabelBox));
		gtk_widget_hide(GTK_WIDGET(ParentSigNameEntryBox));

		gtk_widget_override_background_color(GTK_WIDGET(isMinorLabel), GTK_STATE_FLAG_NORMAL, &gdk_confirm);
	}
	else
	{
		isMinor = true;
		gtk_label_set_text(GTK_LABEL(isMinorLabel), "CLICK HERE IF OVER 18");

		//Reset Name Text
		gtk_label_set_text(GTK_LABEL(parentSigNameLabel), "CLICK HERE TO ENTER NAME");
		gtk_widget_override_background_color(GTK_WIDGET(parentSigNameLabel), GTK_STATE_FLAG_NORMAL, &gdk_unfilled);

		//Show Relevant Widgits
		gtk_widget_show(GTK_WIDGET(parentSigClickSignBox));
		gtk_widget_show(GTK_WIDGET(parentSigNameLabelBox));

		gtk_widget_override_background_color(GTK_WIDGET(isMinorLabel), GTK_STATE_FLAG_NORMAL, &gdk_unfilled);
	}

	return true;


}

void EntryBoxActivate(GtkEntry* entry, gpointer  user_data)
{
	finalizeActiveFields();
}
//Finalize Inputs if clicked outside
gboolean GlobalBoxClick(GtkWidget* widget, GdkEvent* event, gpointer user_data)
{
	finalizeActiveFields();
	return true;
}


//Hide Prompt Text And Show/Activate Signature
gboolean mainSigLabelClicked(GtkWidget* widget, GdkEvent* event, gpointer user_data)
{
	//Clear Other Inputs
	finalizeActiveFields();

	//Hide Label, Show SignBox
	gtk_widget_hide(mainSigClickSignBox);

	gtk_widget_show(mainSigImgBox);
	

	//Highlight Signature Area
	gdk_pixbuf_fill(mainSigPixBufImg, 0xF4F47080);
	gtk_image_set_from_pixbuf(GTK_IMAGE(mainSigImg), mainSigPixBufImg);

	//Activate TP for MainSig
	sigMainActive = true;
	validMainSig = false;

	gtk_widget_grab_focus(GTK_WIDGET(GlobalBox));



	return true;
	
};

//Hide Prompt Text And Show/Activate Signature
gboolean parentSigLabelClicked(GtkWidget* widget, GdkEvent* event, gpointer user_data)
{
	//Clear Other Inputs
	finalizeActiveFields();

	//Hide Label, Show SignBox
	gtk_widget_hide(parentSigClickSignBox);
	gtk_widget_show(parentSigImgBox);


	//Highlight Signature Area
	gdk_pixbuf_fill(parentSigPixBufImg, 0xF4F47080);
	gtk_image_set_from_pixbuf(GTK_IMAGE(parentSigImg), parentSigPixBufImg);

	//Activate TP for MainSig
	sigParentActive = true;
	validParentSig = false;

	gtk_widget_grab_focus(GTK_WIDGET(GlobalBox));



	return true;

};



//Clear and Ractivate Signature
gboolean mainSigClickImgBox(GtkWidget* widget, GdkEvent* event, gpointer user_data)
{
	finalizeActiveFields();

	//Clear And Highlight Signature Area
	gdk_pixbuf_fill(mainSigPixBufImg, 0xF4F47080);
	gtk_image_set_from_pixbuf(GTK_IMAGE(mainSigImg), mainSigPixBufImg);

	//Activate TP for MainSig
	sigMainActive = true;
	validMainSig = false;

	return true;
}


//Clear and Ractivate Signature
gboolean parentSigClickImgBox(GtkWidget* widget, GdkEvent* event, gpointer user_data)
{
	finalizeActiveFields();

	//Clear And Highlight Signature Area
	gdk_pixbuf_fill(parentSigPixBufImg, 0xF4F47080);
	gtk_image_set_from_pixbuf(GTK_IMAGE(parentSigImg), parentSigPixBufImg);

	//Activate TP for MainSig
	sigParentActive = true;
	validParentSig = false;

	return true;
}

//Hide Label and Show Input
gboolean mainSigNameClickLabel(GtkWidget* widget, GdkEvent* event, gpointer user_data)
{
	finalizeActiveFields();

	gtk_widget_hide(mainSigNameLabelBox);
	gtk_widget_show(MainSigNameEntry);

	gtk_widget_grab_focus(GTK_WIDGET(MainSigNameEntry));
	

	return true;
}

//Hide Label and Show Input
gboolean parentSigNameClickLabel(GtkWidget* widget, GdkEvent* event, gpointer user_data)
{
	finalizeActiveFields();

	gtk_widget_hide(parentSigNameLabelBox);
	gtk_widget_show(ParentSigNameEntry);

	gtk_widget_grab_focus(GTK_WIDGET(ParentSigNameEntry));


	return true;
}


	


void activate(GtkApplication* app, gpointer user_data)
{
	//Init Color
	gdk_rgba_parse(&gdk_unfilled, "#f79891");
	gdk_rgba_parse(&gdk_confirm, "#83f939");


	

	//Basic Window Setup
	screen = gdk_screen_get_default();
	int width = gdk_screen_get_width(GDK_SCREEN(screen));

	window = gtk_application_window_new(app);
	gtk_window_set_title(GTK_WINDOW(window), "Window");
	gtk_window_fullscreen(GTK_WINDOW(window));
	gtk_window_set_keep_above(GTK_WINDOW(window), 1);

	
	GtkWidget* mainWindowOverlayBox = gtk_overlay_new();

	//Waiver Base Image Load
	GtkWidget* rawWaiverImg = gtk_image_new_from_file("FS_Waiver_Apr_2019.png");
	rawWaiverPixBuf = gtk_image_get_pixbuf(GTK_IMAGE(rawWaiverImg));

	int height = (int)((float)gdk_pixbuf_get_height(rawWaiverPixBuf) / (float)gdk_pixbuf_get_width(rawWaiverPixBuf) * width);
	rawWaiverPixBuf = gdk_pixbuf_scale_simple(rawWaiverPixBuf, width , height, GDK_INTERP_BILINEAR);
	

	gtk_image_set_from_pixbuf(GTK_IMAGE(rawWaiverImg), rawWaiverPixBuf);

	gtk_widget_set_valign(GTK_WIDGET(rawWaiverImg), GTK_ALIGN_START);

	gtk_overlay_add_overlay(GTK_OVERLAY(mainWindowOverlayBox), rawWaiverImg);

	

	//MainSigClickLabel
	mainSigClickLabel = gtk_label_new("CLICK HERE TO SIGN");
	gtk_widget_set_size_request(GTK_WIDGET(mainSigClickLabel), 280, 25);
	gtk_widget_override_background_color(GTK_WIDGET(mainSigClickLabel), GTK_STATE_FLAG_NORMAL, &gdk_unfilled);

	//MainSigClickLabelBox
	mainSigClickSignBox = gtk_event_box_new();
	gtk_widget_set_halign(GTK_WIDGET(mainSigClickSignBox), GTK_ALIGN_START);
	gtk_widget_set_valign(GTK_WIDGET(mainSigClickSignBox), GTK_ALIGN_START);
	gtk_widget_set_margin_top(GTK_WIDGET(mainSigClickSignBox), 923);
	gtk_widget_set_margin_left(GTK_WIDGET(mainSigClickSignBox), 175);

	g_signal_connect(GTK_WIDGET(mainSigClickSignBox), "button-press-event", G_CALLBACK(mainSigLabelClicked), (gpointer) "MainSigClickLabel");
	

	gtk_container_add(GTK_CONTAINER(mainSigClickSignBox), mainSigClickLabel);
	gtk_overlay_add_overlay(GTK_OVERLAY(mainWindowOverlayBox), mainSigClickSignBox);

	


	

	//MainSigImg
	mainSigPixBufImg = gdk_pixbuf_new(GDK_COLORSPACE_RGB, true, 8, 300, 85);
	gdk_pixbuf_fill(mainSigPixBufImg, 0xFFFFF00); //Fill Transparent
	mainSigImg = gtk_image_new_from_pixbuf(mainSigPixBufImg);


	//MainSigImageBox
	mainSigImgBox = gtk_event_box_new();
	gtk_widget_set_halign(GTK_WIDGET(mainSigImgBox), GTK_ALIGN_START);
	gtk_widget_set_valign(GTK_WIDGET(mainSigImgBox), GTK_ALIGN_START);
	gtk_widget_set_margin_top(GTK_WIDGET(mainSigImgBox), 885);
	gtk_widget_set_margin_left(GTK_WIDGET(mainSigImgBox), 175);

	g_signal_connect(GTK_WIDGET(mainSigImgBox), "button-press-event", G_CALLBACK(mainSigClickImgBox), (gpointer) "MainSigImg");

	gtk_container_add(GTK_CONTAINER(mainSigImgBox), mainSigImg);
	gtk_overlay_add_overlay(GTK_OVERLAY(mainWindowOverlayBox), mainSigImgBox);

	
 	  


	
	

	//MainSigNameClickLabel
	mainSigNameLabel = gtk_label_new("CLICK HERE TO ENTER NAME");
	
	gtk_widget_set_size_request(GTK_WIDGET(mainSigNameLabel), 280, 25);
	gtk_widget_override_background_color(GTK_WIDGET(mainSigNameLabel), GTK_STATE_FLAG_NORMAL, &gdk_unfilled);

	mainSigNameLabelBox = gtk_event_box_new();
	gtk_widget_set_halign(GTK_WIDGET(mainSigNameLabelBox), GTK_ALIGN_START);
	gtk_widget_set_valign(GTK_WIDGET(mainSigNameLabelBox), GTK_ALIGN_START);
	gtk_widget_set_margin_top(GTK_WIDGET(mainSigNameLabelBox), 962);
	gtk_widget_set_margin_left(GTK_WIDGET(mainSigNameLabelBox), 190);

	g_signal_connect(GTK_WIDGET(mainSigNameLabelBox), "button-press-event", G_CALLBACK(mainSigNameClickLabel), (gpointer) "MainSigNameLabel");

	gtk_container_add(GTK_CONTAINER(mainSigNameLabelBox), mainSigNameLabel);
	gtk_overlay_add_overlay(GTK_OVERLAY(mainWindowOverlayBox), mainSigNameLabelBox);



	//MainSigNameEntry
	MainSigNameEntry = gtk_entry_new();
	gtk_widget_set_size_request(GTK_WIDGET(mainSigNameLabel), 280, 25);
	gtk_widget_set_halign(GTK_WIDGET(MainSigNameEntry), GTK_ALIGN_START);
	gtk_widget_set_valign(GTK_WIDGET(MainSigNameEntry), GTK_ALIGN_START);
	gtk_widget_set_margin_top(GTK_WIDGET(MainSigNameEntry), 962);
	gtk_widget_set_margin_left(GTK_WIDGET(MainSigNameEntry), 190);

	g_signal_connect(GTK_WIDGET(MainSigNameEntry), "activate", G_CALLBACK(EntryBoxActivate), (gpointer) "MainSigEntry");

	gtk_overlay_add_overlay(GTK_OVERLAY(mainWindowOverlayBox), MainSigNameEntry);







	//parentSigClickLabel
	parentSigClickLabel = gtk_label_new("CLICK HERE TO SIGN");
	gtk_widget_set_size_request(GTK_WIDGET(parentSigClickLabel), 280, 25);
	gtk_widget_override_background_color(GTK_WIDGET(parentSigClickLabel), GTK_STATE_FLAG_NORMAL, &gdk_unfilled);

	//parentSigClickLabelBox
	parentSigClickSignBox = gtk_event_box_new();
	gtk_widget_set_halign(GTK_WIDGET(parentSigClickSignBox), GTK_ALIGN_START);
	gtk_widget_set_valign(GTK_WIDGET(parentSigClickSignBox), GTK_ALIGN_START);
	gtk_widget_set_margin_top(GTK_WIDGET(parentSigClickSignBox), 923+115);
	gtk_widget_set_margin_left(GTK_WIDGET(parentSigClickSignBox), 175);

	g_signal_connect(GTK_WIDGET(parentSigClickSignBox), "button-press-event", G_CALLBACK(parentSigLabelClicked), (gpointer) "MainSigClickLabel");


	gtk_container_add(GTK_CONTAINER(parentSigClickSignBox), parentSigClickLabel);
	gtk_overlay_add_overlay(GTK_OVERLAY(mainWindowOverlayBox), parentSigClickSignBox);


	//parentSigImg
	parentSigPixBufImg = gdk_pixbuf_new(GDK_COLORSPACE_RGB, true, 8, 300, 85);
	gdk_pixbuf_fill(parentSigPixBufImg, 0xFFFFF00); //Fill Transparent
	parentSigImg = gtk_image_new_from_pixbuf(parentSigPixBufImg);


	//parentSigImageBox
	parentSigImgBox = gtk_event_box_new();
	gtk_widget_set_halign(GTK_WIDGET(parentSigImgBox), GTK_ALIGN_START);
	gtk_widget_set_valign(GTK_WIDGET(parentSigImgBox), GTK_ALIGN_START);
	gtk_widget_set_margin_top(GTK_WIDGET(parentSigImgBox), 885 + 115);
	gtk_widget_set_margin_left(GTK_WIDGET(parentSigImgBox), 175);

	g_signal_connect(GTK_WIDGET(parentSigImgBox), "button-press-event", G_CALLBACK(parentSigClickImgBox), (gpointer) "parentSigImg");

	gtk_container_add(GTK_CONTAINER(parentSigImgBox), parentSigImg);
	gtk_overlay_add_overlay(GTK_OVERLAY(mainWindowOverlayBox), parentSigImgBox);


	//ParentSigNameClickLabel
	parentSigNameLabel = gtk_label_new("CLICK HERE TO ENTER NAME");

	gtk_widget_set_size_request(GTK_WIDGET(parentSigNameLabel), 280, 25);
	gtk_widget_override_background_color(GTK_WIDGET(parentSigNameLabel), GTK_STATE_FLAG_NORMAL, &gdk_unfilled);

	parentSigNameLabelBox = gtk_event_box_new();
	gtk_widget_set_halign(GTK_WIDGET(parentSigNameLabelBox), GTK_ALIGN_START);
	gtk_widget_set_valign(GTK_WIDGET(parentSigNameLabelBox), GTK_ALIGN_START);
	gtk_widget_set_margin_top(GTK_WIDGET(parentSigNameLabelBox), 962+115);
	gtk_widget_set_margin_left(GTK_WIDGET(parentSigNameLabelBox), 190);

	g_signal_connect(GTK_WIDGET(parentSigNameLabelBox), "button-press-event", G_CALLBACK(parentSigNameClickLabel), (gpointer) "ParentSigNameLabel");

	gtk_container_add(GTK_CONTAINER(parentSigNameLabelBox), parentSigNameLabel);
	gtk_overlay_add_overlay(GTK_OVERLAY(mainWindowOverlayBox), parentSigNameLabelBox);



	//ParentSigNameEntry
	ParentSigNameEntry = gtk_entry_new();
	gtk_widget_set_size_request(GTK_WIDGET(parentSigNameLabel), 280, 25);
	gtk_widget_set_halign(GTK_WIDGET(ParentSigNameEntry), GTK_ALIGN_START);
	gtk_widget_set_valign(GTK_WIDGET(ParentSigNameEntry), GTK_ALIGN_START);
	gtk_widget_set_margin_top(GTK_WIDGET(ParentSigNameEntry), 962+115);
	gtk_widget_set_margin_left(GTK_WIDGET(ParentSigNameEntry), 190);

	g_signal_connect(GTK_WIDGET(ParentSigNameEntry), "activate", G_CALLBACK(EntryBoxActivate), (gpointer) "ParentSigEntry");

	gtk_overlay_add_overlay(GTK_OVERLAY(mainWindowOverlayBox), ParentSigNameEntry);



	//Over 18 Configm

	isMinorLabel = gtk_label_new("CLICK HERE IF OVER 18");

	gtk_widget_set_size_request(GTK_WIDGET(isMinorLabel), 225, 25);
	gtk_widget_override_background_color(GTK_WIDGET(isMinorLabel), GTK_STATE_FLAG_NORMAL, &gdk_unfilled);

	isMinorBox = gtk_event_box_new();
	gtk_widget_set_halign(GTK_WIDGET(isMinorBox), GTK_ALIGN_START);
	gtk_widget_set_valign(GTK_WIDGET(isMinorBox), GTK_ALIGN_START);
	gtk_widget_set_margin_top(GTK_WIDGET(isMinorBox), 962+115);
	gtk_widget_set_margin_left(GTK_WIDGET(isMinorBox), 500);

	g_signal_connect(GTK_WIDGET(isMinorBox), "button-press-event", G_CALLBACK(isMinorClick), (gpointer) "MinorClick");

	gtk_container_add(GTK_CONTAINER(isMinorBox), isMinorLabel);
	gtk_overlay_add_overlay(GTK_OVERLAY(mainWindowOverlayBox), isMinorBox);






	//Date Labels
	time_t now = time(0);
	tm* ltm = localtime(&now);

	char datestring[128];
	
	strftime(datestring, sizeof(datestring), "%B %d, %Y", ltm);
	
	mainDate = gtk_label_new(datestring);

	gtk_widget_set_halign(GTK_WIDGET(mainDate), GTK_ALIGN_START);
	gtk_widget_set_valign(GTK_WIDGET(mainDate), GTK_ALIGN_START);
	gtk_widget_set_margin_top(GTK_WIDGET(mainDate), 930);
	gtk_widget_set_margin_left(GTK_WIDGET(mainDate), 580);

	gtk_overlay_add_overlay(GTK_OVERLAY(mainWindowOverlayBox), mainDate);



	parentDate = gtk_label_new(datestring);

	gtk_widget_set_halign(GTK_WIDGET(parentDate), GTK_ALIGN_START);
	gtk_widget_set_valign(GTK_WIDGET(parentDate), GTK_ALIGN_START);
	gtk_widget_set_margin_top(GTK_WIDGET(parentDate), 930+115);
	gtk_widget_set_margin_left(GTK_WIDGET(parentDate), 580);

	gtk_overlay_add_overlay(GTK_OVERLAY(mainWindowOverlayBox), parentDate);


	
	//Submit Button
	GtkWidget* submit = gtk_button_new();
	gtk_button_set_label(GTK_BUTTON(submit), "SUBMIT");
	gtk_widget_set_size_request(GTK_WIDGET(submit), 200, 100);
	gtk_widget_set_valign(GTK_WIDGET(submit), GTK_ALIGN_START);
	gtk_widget_set_halign(GTK_WIDGET(submit), GTK_ALIGN_START);
	gtk_widget_set_margin_left(GTK_WIDGET(submit), 50);
	gtk_widget_set_margin_top(GTK_WIDGET(submit), 1320);

	gtk_overlay_add_overlay(GTK_OVERLAY(mainWindowOverlayBox), submit);

	g_signal_connect(GTK_WIDGET(submit), "clicked", G_CALLBACK(SubmitClicked), (gpointer) "SubmitClick");

	//Cancel Button
	GtkWidget* cancel = gtk_button_new();
	gtk_button_set_label(GTK_BUTTON(cancel), "CANCEL");
	gtk_widget_set_size_request(GTK_WIDGET(cancel), 200, 100);
	gtk_widget_set_valign(GTK_WIDGET(cancel), GTK_ALIGN_START);
	gtk_widget_set_halign(GTK_WIDGET(cancel), GTK_ALIGN_START);
	gtk_widget_set_margin_left(GTK_WIDGET(cancel), 650);
	gtk_widget_set_margin_top(GTK_WIDGET(cancel), 1320);

	gtk_overlay_add_overlay(GTK_OVERLAY(mainWindowOverlayBox), cancel);

	g_signal_connect(GTK_WIDGET(cancel), "clicked", G_CALLBACK(CancelClicked), (gpointer) "CancelClick");
	

	//Status Label

    statusLabel = gtk_label_new("Thank You For Your Submission.");
	gtk_widget_set_size_request(GTK_WIDGET(statusLabel), 400, 25);
	gtk_widget_override_background_color(GTK_WIDGET(statusLabel), GTK_STATE_FLAG_NORMAL, &gdk_confirm);


	gtk_widget_set_valign(GTK_WIDGET(statusLabel), GTK_ALIGN_START);
	gtk_widget_set_halign(GTK_WIDGET(statusLabel), GTK_ALIGN_CENTER);
	gtk_widget_set_margin_top(GTK_WIDGET(statusLabel), 1280);

	gtk_overlay_add_overlay(GTK_OVERLAY(mainWindowOverlayBox), statusLabel);






	//Event Box to catch Stray Clicks
	GlobalBox = gtk_event_box_new();
	gtk_widget_set_size_request(GTK_WIDGET(GlobalBox), gdk_screen_get_width(GDK_SCREEN(screen)), gdk_screen_get_height(GDK_SCREEN(screen)));
	g_signal_connect(GTK_WIDGET(GlobalBox), "button-press-event", G_CALLBACK(GlobalBoxClick), (gpointer) "GlobalBoxClick");
	g_signal_connect(window, "key-press-event", G_CALLBACK(GlobalKeyPress), (gpointer) "GlobalKey");
	
	//Show All the Things
	gtk_container_add(GTK_CONTAINER(GlobalBox), mainWindowOverlayBox);
	gtk_container_add(GTK_CONTAINER(window), GlobalBox);
	
	
	
	gtk_widget_show_all(window);

	//Hide Inputs To Start
	gtk_widget_hide(GTK_WIDGET(mainSigImgBox));
	gtk_widget_hide(GTK_WIDGET(MainSigNameEntry));
	gtk_widget_hide(GTK_WIDGET(parentSigImgBox));
	gtk_widget_hide(GTK_WIDGET(ParentSigNameEntry));
	gtk_widget_hide(GTK_WIDGET(statusLabel));

	

	


	//Start Touchpad handler
	g_timeout_add(500, message_handler, NULL);
	g_timeout_add(200, touchpad_handler, NULL);
	g_timeout_add(20000, date_handler, NULL);
	

	//EMasks
	gdk_window_set_events(GDK_WINDOW(window), GDK_KEY_PRESS_MASK);
	gdk_window_set_events(GDK_WINDOW(window), GDK_STRUCTURE_MASK);
	gdk_window_set_events(GDK_WINDOW(window), GDK_BUTTON_PRESS_MASK);

	g_signal_connect(window, "destroy-event", G_CALLBACK(mainCleanup), NULL);
	

	gtk_main();
	
}





int main(int    argc,
	char** argv)
{

	

	GtkApplication* app;
	int status;
	app = gtk_application_new("org.gtk.example", G_APPLICATION_FLAGS_NONE);
	g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
	status = g_application_run(G_APPLICATION(app), argc, argv);
	g_object_unref(app);

	return status;
}


