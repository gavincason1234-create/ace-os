# 🟦 START HERE — Put ACE OS on your laptop

Do all of this **on the laptop**, with your **blue flash drive plugged in**.

---

## PART 1 — Download 2 things

**1.** Click this and let it finish downloading (it's big — take a break ☕):

### 👉 [DOWNLOAD ACE OS (click here)](https://github.com/gavincason1234-create/ace-os/releases/latest/download/ace-os-amd64.iso)

> ⚠️ If the browser says the file *"isn't commonly downloaded"* or asks if you
> trust it — click the **⋯** dots on the warning, then **Keep**, then
> **Keep anyway**. That warning is normal for a homemade system like this one.
> The download bubble is in the **top-right corner** of the browser.

**2.** Click this and download Rufus (the small program that loads the drive):

### 👉 [DOWNLOAD RUFUS (click here)](https://github.com/pbatard/rufus/releases/latest)

On that page, click the first file that ends in **.exe** (like `rufus-4.9.exe`).

---

## PART 2 — Load ACE OS onto the blue drive (~10 min)

**3.** Open your **Downloads** folder. Double-click **rufus**. Click **Yes** if asked.

> ### ⚠️ Drive not showing in Rufus? Read this — it's a checkbox.
>
> Rufus **hides big drives on purpose** (anything over ~64 GB is treated as a
> "USB Hard Drive" so people don't wipe their backup disks by accident).
> A 128 GB stick will be invisible until you turn that off:
>
> 1. In the middle of the Rufus window, click the small words
>    **"Show advanced drive properties"**
> 2. Tick the box **"List USB Hard Drives"**
> 3. Your drive appears in the top box. ✔
>
> Still nothing? Close Rufus, unplug/replug the drive, reopen Rufus.

**4.** The top box in Rufus should show your **blue drive**. ✔

**5.** Click **SELECT** → pick the file **ace-os-amd64** → click Open.

**6.** Click **START** at the bottom. Click **OK** on any pop-ups.
(If it asks "ISO mode or DD mode" — pick **DD mode**.)

**7.** Wait until the green bar says **READY**. Close Rufus. The drive is loaded. ✅

---

## PART 3 — Make the laptop start from the blue drive

**8.** Leave the blue drive plugged in. **Restart** the laptop.

**9.** The second the screen goes black, **tap the `F2` key over and over**
until a settings screen appears.

**10.** With the arrow keys, go to **Security** → find **Secure Boot** →
change it to **Disabled**.

**11.** Press **F10**, then Enter. The laptop restarts.

**12.** The second it restarts, **tap the `Esc` key over and over**.
A small menu appears.

**13.** Pick the line with the blue drive's name and press Enter.

---

## PART 4 — Install it (this ERASES Windows — that's the plan)

**14.** Wait a minute or two. A desktop appears — that's ACE OS,
running from the blue drive!

**15.** Double-click **Install** on the desktop.

**16.** Answer the questions: language, time zone, and a **username and
password you will remember** (write them down!).

**17.** When it asks about the disk: pick **Erase disk** → click **Install**
→ wait about 20 minutes.

**18.** Click **Restart now** when it finishes, and **pull out the blue
drive** when the screen goes dark.

---

# 🎉 That's it!

The laptop starts up as an ACE OS Linux computer. Connect to Wi-Fi with the
icon in the top corner, and you're done.

---

## 🧠 Turn on the AI (after it's installed)

ACE OS has its own AI that runs **on the laptop itself** — no account, no
subscription, and after the one-time download it works with the internet off.

**19.** Connect to Wi-Fi.

**20.** Open **ACE AI** from the applications menu (or open a terminal and
type `ace`).

**21.** Type `/setup` and press Enter. It downloads the AI brain
(about 1.1 GB, one time only). Then just talk to it.

Useful things to type inside ACE:

| Type this | What it does |
| --- | --- |
| *(anything)* | Ask the AI a question |
| `/sys` | Memory, CPU, disk, uptime |
| `/model 0.5b` | Switch to the smaller, faster brain |
| `/help` | All commands |
| `/exit` | Quit |

**Speed check, honestly:** this laptop's processor is modest, so the AI
replies at a few words per second. Great for questions, explanations, and
writing help — not for huge documents.

**Something looks different than these steps?** Stop, take a photo of the
screen, and ask for help — don't guess.
