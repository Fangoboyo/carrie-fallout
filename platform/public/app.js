document.addEventListener("DOMContentLoaded", async () => {
    const appContent = document.getElementById("app-content");

    try {
        const res = await fetch("/api/me");
        const data = await res.json();
        
        if (data.user) {
            renderDashboard(data.user);
        } else {
            renderLogin();
        }
    } catch (err) {
        console.error(err);
        renderLogin();
    }

    function renderLogin() {
        appContent.innerHTML = `
            <div class="polaroid tape-pink" style="max-width: 400px; margin: 0 auto; text-align: center;">
                <h2 class="handwriting">Welcome!</h2>
                <p>Join the scrapbook to share and view memories.</p>
                <button class="login-btn" onclick="window.location.href='/auth/google'">
                    <img src="https://upload.wikimedia.org/wikipedia/commons/5/53/Google_%22G%22_Logo.svg" alt="G" width="20" height="20">
                    Sign in with Google
                </button>
            </div>
        `;
    }

    function renderDashboard(user) {
        appContent.innerHTML = `
            <div class="polaroid tape-blue" style="margin-bottom: 2rem;">
                <div style="display: flex; justify-content: space-between; align-items: flex-start;">
                    <div>
                        <h2 class="handwriting" style="margin-top: 0;">Hi, ${user.name}!</h2>
                        <p style="margin-bottom: 0.5rem; font-size: 0.9rem; color: #666;">Your API Key (keep it secret!):</p>
                        <div class="api-key-box" onclick="navigator.clipboard.writeText('${user.api_key}'); alert('Copied!')" title="Click to copy">
                            ${user.api_key}
                        </div>
                    </div>
                    <button onclick="window.location.href='/api/logout'" style="background: #e0e0e0; color: #333; font-size: 0.8rem; padding: 0.5rem 1rem;">Log Out</button>
                </div>
                
                <div style="margin-top: 2rem; padding-top: 2rem; border-top: 1px dashed #eee;">
                    <h3 class="handwriting">Add a Memory</h3>
                    <form id="upload-form" class="upload-section">
                        <input type="text" id="video-title" placeholder="Title for your video..." style="padding: 0.5rem; width: 100%; max-width: 300px; border: 1px solid #ccc; font-family: 'Inter', sans-serif;" required>
                        <input type="file" id="video-file" accept="video/*" required>
                        <button type="submit" id="upload-btn">Upload to Drive</button>
                        <p id="upload-status" style="font-size: 0.9rem; display: none;"></p>
                    </form>
                </div>
            </div>

            <div style="display: flex; align-items: center; justify-content: space-between;">
                <h2 class="handwriting" style="font-size: 2rem;">Friends' Memories</h2>
                <button onclick="loadFeed()" style="background: transparent; color: #555; padding: 0; text-decoration: underline;">Refresh</button>
            </div>
            
            <div id="video-feed" class="video-grid">
                <p>Loading memories...</p>
            </div>
        `;

        const form = document.getElementById("upload-form");
        form.addEventListener("submit", async (e) => {
            e.preventDefault();
            
            const fileInput = document.getElementById("video-file");
            const titleInput = document.getElementById("video-title");
            const btn = document.getElementById("upload-btn");
            const status = document.getElementById("upload-status");
            
            if (!fileInput.files[0]) return;

            btn.disabled = true;
            btn.innerText = "Uploading...";
            status.style.display = "block";
            status.innerText = "Uploading to Google Drive...";
            status.style.color = "#666";

            const formData = new FormData();
            formData.append("video", fileInput.files[0]);
            formData.append("title", titleInput.value);

            try {
                const res = await fetch("/api/upload", {
                    method: "POST",
                    headers: {
                        "Authorization": "Bearer " + user.api_key
                    },
                    body: formData
                });

                if (res.ok) {
                    status.innerText = "Upload successful! ✅";
                    status.style.color = "green";
                    form.reset();
                    loadFeed(); // refresh feed
                } else {
                    const text = await res.text();
                    status.innerText = "Error: " + text;
                    status.style.color = "red";
                }
            } catch (err) {
                status.innerText = "Upload failed.";
                status.style.color = "red";
            } finally {
                btn.disabled = false;
                btn.innerText = "Upload to Drive";
            }
        });

        loadFeed();
    }

    async function loadFeed() {
        const feedContainer = document.getElementById("video-feed");
        try {
            const res = await fetch("/api/videos");
            const videos = await res.json();

            if (videos.length === 0) {
                feedContainer.innerHTML = "<p style='color: #888;'>No memories added yet. Be the first!</p>";
                return;
            }

            feedContainer.innerHTML = videos.map((v, i) => {
                const tapes = ['tape-pink', 'tape-blue', ''];
                const tape = tapes[i % tapes.length];
                
                return `
                    <div class="polaroid ${tape} video-card">
                        <div class="video-wrapper">
                            <iframe src="https://drive.google.com/file/d/${v.google_drive_file_id}/preview" width="100%" height="100%" frameborder="0" allow="autoplay" allowfullscreen></iframe>
                        </div>
                        <div class="caption">
                            <p class="handwriting" style="font-size: 1.2rem; margin-bottom: 0.2rem;">${escapeHTML(v.title)}</p>
                            <p style="font-size: 0.8rem; color: #888; margin: 0;">Uploaded by <strong>${escapeHTML(v.uploader_name)}</strong></p>
                        </div>
                    </div>
                `;
            }).join("");
        } catch (err) {
            feedContainer.innerHTML = "<p style='color: red;'>Failed to load feed.</p>";
        }
    }

    function escapeHTML(str) {
        return str.replace(/[&<>'"]/g, 
            tag => ({
                '&': '&amp;',
                '<': '&lt;',
                '>': '&gt;',
                "'": '&#39;',
                '"': '&quot;'
            }[tag] || tag)
        );
    }
});
