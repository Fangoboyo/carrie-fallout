import { serve } from "bun";
import { getAuthUrl, getOAuth2Client } from "./google";
import db from "./db";
import { google } from "googleapis";
import { Readable } from "stream";

function parseCookies(cookieHeader: string | null) {
  if (!cookieHeader) return {};
  return cookieHeader.split(";").reduce((res, item) => {
    const data = item.trim().split("=");
    return { ...res, [data[0]]: data[1] };
  }, {} as Record<string, string>);
}

serve({
  port: process.env.PORT || 3000,
  async fetch(req) {
    const url = new URL(req.url);
    const cookies = parseCookies(req.headers.get("Cookie"));
    
    // --- API Routes ---
    
    if (url.pathname === "/auth/google") {
      return Response.redirect(getAuthUrl());
    }

    if (url.pathname === "/auth/callback") {
      const code = url.searchParams.get("code");
      if (!code) return new Response("No code provided", { status: 400 });

      try {
        const oauth2Client = getOAuth2Client();
        const { tokens } = await oauth2Client.getToken(code);
        oauth2Client.setCredentials(tokens);

        const oauth2 = google.oauth2({ version: "v2", auth: oauth2Client });
        const userInfo = await oauth2.userinfo.get();
        
        const googleId = userInfo.data.id!;
        const email = userInfo.data.email;
        const name = userInfo.data.name;
        const picture = userInfo.data.picture;

        const apiKey = "vs_" + crypto.randomUUID().replace(/-/g, "");

        const existing = db.query("SELECT id, api_key FROM users WHERE google_id = ?").get(googleId) as any;
        let userId = existing?.id || crypto.randomUUID();
        
        if (existing) {
          db.query(`
            UPDATE users 
            SET access_token = ?, refresh_token = COALESCE(?, refresh_token)
            WHERE id = ?
          `).run(tokens.access_token!, tokens.refresh_token || null, userId);
        } else {
          db.query(`
            INSERT INTO users (id, google_id, email, name, picture, access_token, refresh_token, api_key)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?)
          `).run(userId, googleId, email, name, picture, tokens.access_token!, tokens.refresh_token || "", apiKey);
        }

        const headers = new Headers();
        headers.append("Set-Cookie", `session_user_id=${userId}; Path=/; HttpOnly; SameSite=Lax`);
        headers.append("Location", "/");
        return new Response(null, { status: 302, headers });
      } catch (err) {
        console.error(err);
        return new Response("Auth failed", { status: 500 });
      }
    }

    if (url.pathname === "/api/me") {
      const userId = cookies["session_user_id"];
      if (!userId) return new Response(JSON.stringify({ user: null }), { headers: { "Content-Type": "application/json" } });
      
      const user = db.query("SELECT id, name, picture, api_key FROM users WHERE id = ?").get(userId) as any;
      return new Response(JSON.stringify({ user }), { headers: { "Content-Type": "application/json" } });
    }

    if (url.pathname === "/api/upload" && req.method === "POST") {
      const authHeader = req.headers.get("Authorization");
      if (!authHeader || !authHeader.startsWith("Bearer ")) {
        return new Response("Unauthorized", { status: 401 });
      }
      
      const apiKey = authHeader.split(" ")[1];
      const user = db.query("SELECT id, access_token, refresh_token, name FROM users WHERE api_key = ?").get(apiKey) as any;
      if (!user) return new Response("Invalid API Key", { status: 401 });

      const formData = await req.formData();
      const file = formData.get("video") as File;
      const title = (formData.get("title") as string) || file.name;
      
      if (!file) return new Response("No file uploaded", { status: 400 });

      const oauth2Client = getOAuth2Client();
      oauth2Client.setCredentials({
        access_token: user.access_token,
        refresh_token: user.refresh_token
      });

      const drive = google.drive({ version: "v3", auth: oauth2Client });
      
      try {
        const response = await drive.files.create({
          requestBody: { name: title },
          media: {
            mimeType: file.type,
            body: Readable.fromWeb(file.stream() as any)
          },
          fields: "id"
        });

        const fileId = response.data.id;

        db.query(`
          INSERT INTO videos (id, user_id, google_drive_file_id, title)
          VALUES (?, ?, ?, ?)
        `).run(crypto.randomUUID(), user.id, fileId, title);

        return new Response(JSON.stringify({ success: true, fileId }), {
          headers: { "Content-Type": "application/json" }
        });
      } catch (e: any) {
        console.error(e);
        return new Response(JSON.stringify({ error: e.message }), { status: 500, headers: { "Content-Type": "application/json" } });
      }
    }

    if (url.pathname === "/api/videos") {
      const videos = db.query(`
        SELECT videos.id, videos.title, videos.google_drive_file_id, videos.uploaded_at, users.name as uploader_name
        FROM videos
        JOIN users ON videos.user_id = users.id
        ORDER BY videos.uploaded_at DESC
        LIMIT 20
      `).all();
      return new Response(JSON.stringify(videos), { headers: { "Content-Type": "application/json" } });
    }

    if (url.pathname === "/api/logout") {
      const headers = new Headers();
      headers.append("Set-Cookie", `session_user_id=; Path=/; HttpOnly; Max-Age=0`);
      headers.append("Location", "/");
      return new Response(null, { status: 302, headers });
    }

    // --- Static File Serving ---
    const publicPath = process.cwd() + "/client/dist";
    
    // Serve file if it exists, otherwise fallback to index.html
    const filePath = url.pathname === "/" ? "/index.html" : url.pathname;
    const file = Bun.file(publicPath + filePath);
    
    if (await file.exists()) {
      return new Response(file);
    }

    // SPA Fallback
    const indexFile = Bun.file(publicPath + "/index.html");
    if (await indexFile.exists()) {
      return new Response(indexFile);
    }

    return new Response("Not Found", { status: 404 });
  }
});

console.log("Server running on http://localhost:3000");
