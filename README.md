# FileWiz

FileWiz (File Wizard) is a file manager that uses mouse and
keyboard in a hybrid manner to make manipulating files
and directories faster and easier. It is written in C
using Raylib and Clay (C-Layout) for rendering GUI.

## Default controls
### Q
Exit the application immediately.

### W
Go to your home directory (~), typically /home/<your_username>.

### H
Toggle 'hide' mode. On by default. In 'hide' mode, all files
and directories starting with dot (.) will not be shown. Useful
if you're browsing your home directory, where a lot of dotfiles
are usually located.

### E
'Unserious' accept. Used to accept deleting a single file.

### Enter
'Serious' accept. Used to accept deleting a group of files.
Hitting enter requires you to either lift your right arm from
the mouse, or move your left arm. Either way, this prevents
disastrous misclicks.

### Escape
Default key for cancelling creating files, cancelling selection, etc.

### A
View the parent directory of current directory.

### F
Create a new directory. A prompt will be shown. Press enter
to accept name, press escape to cancel.

### S
Select key. Hold it, press left mouse button and hover it
over the files you want to select. Selected files will not
be de-selected even if you leave the directory.
To de-select them, press escape.

### R
Delete key. Press without selecting a file and a popup menu
will show for you to chose *one* file that will be deleted.
Press after selecting any number of files, and a prompt
asking if you want to delete these files will show.

### G
Move key. Select some files, change view to any directory, press
G to move all of the selected files into the current directory.

### T
Copy (or paste) key. Select some files, change view to any directory,
press T to copy all of the selected files into the current directory.

## Customization
Customizing controls and color schemes can be done directly
in the 'main.c' source file. Here's how it looks:

```C
#define K_QUIT KEY_Q
#define K_HOME KEY_W
#define K_HIDE KEY_H
#define K_ACCEPT KEY_E
#define K_ACCEPT_SERIOUS KEY_ENTER
#define K_CANCEL KEY_ESCAPE
#define K_BACK KEY_A
#define K_MAKEDIR KEY_F
#define K_SELECT KEY_S
#define K_DEL KEY_R
#define K_MOVE KEY_G
#define K_COPY KEY_T

/* Colorscheme and other UI configuration */
#define BOTTOM_BAR
#define INIT_SCRW 900
#define INIT_SCRH 600
<...>
```
Look up key names in 'raylib.h'; there are generally pretty
intuitive.

## Building && usage
You should have raylib install as a system library. Clay is distributed
alongside this project. Run './build.sh'. Ta-da! The project is built.
Run filewiz. Notice that, for now, the font used by FileWiz must
be located in the same folder as it is run in. You can use a logical
link to run it from somewhere else (like /usr/bin/). This will be
fixed in future.

## About Clay
Thanks to @nicbarker for writing this library! It is easy to use
and fits my needs perfectly. The following text is the Z-lib license
used by Clay:
```
1. The origin of this software must not be misrepresented; you must not
claim that you wrote the original software. If you use this software in a
product, an acknowledgment in the product documentation would be
appreciated but is not required.

2. Altered source versions must be plainly marked as such, and must not
be misrepresented as being the original software.

3. This notice may not be removed or altered from any source
distribution.
```
Z-lib license applies only to 'clay.h'. The license of the rest of the
project is GNU GPL-2 (you can find it in 'LICENSE').
