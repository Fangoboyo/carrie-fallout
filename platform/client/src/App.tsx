import { useEffect, useState } from 'react'
import { Polaroid } from '@/components/Polaroid'
import { Button } from '@/components/ui/button'
import { Input } from '@/components/ui/input'
import { Camera, LogOut, Upload, RefreshCw } from 'lucide-react'

function App() {
  const [user, setUser] = useState<any>(null)
  const [loading, setLoading] = useState(true)
  const [videos, setVideos] = useState<any[]>([])
  
  // Upload State
  const [uploading, setUploading] = useState(false)
  const [file, setFile] = useState<File | null>(null)
  const [title, setTitle] = useState("")
  const [status, setStatus] = useState({ type: '', msg: '' })

  useEffect(() => {
    fetch("/api/me")
      .then(r => r.json())
      .then(data => {
        if (data.user) {
          setUser(data.user)
          loadFeed()
        }
        setLoading(false)
      })
      .catch(() => setLoading(false))
  }, [])

  const loadFeed = () => {
    fetch("/api/videos")
      .then(r => r.json())
      .then(data => setVideos(data))
      .catch(console.error)
  }

  const handleUpload = async (e: React.FormEvent) => {
    e.preventDefault()
    if (!file || !user) return

    setUploading(true)
    setStatus({ type: 'info', msg: 'Uploading to Google Drive...' })

    const formData = new FormData()
    formData.append("video", file)
    formData.append("title", title)

    try {
      const res = await fetch("/api/upload", {
        method: "POST",
        headers: { "Authorization": "Bearer " + user.api_key },
        body: formData
      })

      if (res.ok) {
        setStatus({ type: 'success', msg: 'Upload successful! ✅' })
        setFile(null)
        setTitle("")
        
        // Reset file input element since controlled state doesn't wipe file inputs
        const fileInput = document.getElementById('video-upload') as HTMLInputElement
        if(fileInput) fileInput.value = ''
        
        loadFeed()
      } else {
        const text = await res.text()
        setStatus({ type: 'error', msg: "Error: " + text })
      }
    } catch (err) {
      setStatus({ type: 'error', msg: 'Upload failed.' })
    } finally {
      setUploading(false)
    }
  }

  if (loading) {
    return (
      <div className="flex min-h-screen items-center justify-center">
        <p className="handwriting text-3xl text-gray-500 animate-pulse">Loading pages...</p>
      </div>
    )
  }

  if (!user) {
    return (
      <div className="flex min-h-screen flex-col items-center pt-24 space-y-12 max-w-4xl mx-auto px-4">
        <Polaroid delay={0.1} className="text-center w-full max-w-md">
          <h1 className="text-3xl font-bold mb-2">Our Video Scrapbook 📸</h1>
          <p className="handwriting text-gray-500 text-2xl">Memories to keep!</p>
        </Polaroid>

        <Polaroid tapeColor="pink" rotation={2} delay={0.3} className="text-center w-full max-w-md">
          <h2 className="handwriting text-4xl mb-4">Welcome!</h2>
          <p className="text-gray-600 mb-8">Join the scrapbook to share and view memories.</p>
          <Button onClick={() => window.location.href='/auth/google'} className="w-full h-12 text-md" variant="outline">
            <img src="https://upload.wikimedia.org/wikipedia/commons/5/53/Google_%22G%22_Logo.svg" alt="G" className="w-5 h-5 mr-3" />
            Sign in with Google
          </Button>
        </Polaroid>
      </div>
    )
  }

  return (
    <div className="flex flex-col items-center pt-12 pb-24 space-y-12 max-w-5xl mx-auto px-4 overflow-hidden">
      
      {/* Header */}
      <Polaroid className="w-full flex flex-col md:flex-row justify-between items-center py-6 px-8 gap-4">
        <div className="text-center md:text-left">
          <h1 className="text-2xl font-bold">Our Video Scrapbook 📸</h1>
          <p className="handwriting text-gray-500 text-2xl">Memories to keep!</p>
        </div>
        <Button variant="ghost" onClick={() => window.location.href='/api/logout'}>
          <LogOut className="w-4 h-4 mr-2" /> Log Out
        </Button>
      </Polaroid>

      {/* Dashboard Top Row */}
      <div className="grid grid-cols-1 md:grid-cols-2 gap-8 w-full">
        <Polaroid tapeColor="blue" rotation={-1} delay={0.1} className="h-full">
          <h2 className="handwriting text-4xl mb-4">Hi, {user.name}!</h2>
          <div className="bg-gray-50 p-4 rounded-md border border-dashed border-gray-300 mt-4">
            <p className="text-sm text-gray-500 mb-2 font-medium">Your API Key (keep it secret!):</p>
            <p 
              className="font-mono text-xs break-all bg-white p-3 rounded border cursor-pointer hover:bg-gray-50 transition-colors"
              onClick={() => { navigator.clipboard.writeText(user.api_key); alert('Copied!') }}
              title="Click to copy"
            >
              {user.api_key}
            </p>
          </div>
        </Polaroid>

        <Polaroid tapeColor="yellow" rotation={1.5} delay={0.2} className="h-full">
          <h3 className="handwriting text-3xl mb-6 flex items-center gap-2">
            <Camera className="w-6 h-6" /> Add a Memory
          </h3>
          <form onSubmit={handleUpload} className="space-y-4">
            <Input 
              value={title} 
              onChange={e => setTitle(e.target.value)} 
              placeholder="Title for your video..." 
              required 
            />
            <Input 
              id="video-upload"
              type="file" 
              accept="video/*" 
              onChange={e => setFile(e.target.files?.[0] || null)}
              required 
              className="cursor-pointer"
            />
            <Button type="submit" disabled={uploading || !file} className="w-full">
              {uploading ? 'Uploading...' : <><Upload className="w-4 h-4 mr-2"/> Upload to Drive</>}
            </Button>
            {status.msg && (
              <p className={`text-sm ${status.type === 'error' ? 'text-red-500' : 'text-green-600'}`}>
                {status.msg}
              </p>
            )}
          </form>
        </Polaroid>
      </div>

      {/* Feed Section */}
      <div className="w-full pt-8">
        <div className="flex justify-between items-end mb-12">
          <h2 className="handwriting text-5xl">Friends' Memories</h2>
          <Button variant="ghost" size="sm" onClick={loadFeed}>
            <RefreshCw className="w-4 h-4 mr-2" /> Refresh
          </Button>
        </div>

        {videos.length === 0 ? (
          <p className="text-gray-500 italic text-center py-12">No memories added yet. Be the first!</p>
        ) : (
          <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-3 gap-8">
            {videos.map((v, i) => {
              const tapes = ['pink', 'blue', 'yellow'] as const;
              const tapeColor = tapes[i % 3];
              const rotation = (i % 2 === 0 ? 1 : -1) * (1.5 + Math.random());
              
              return (
                <Polaroid key={v.id} tapeColor={tapeColor} rotation={rotation} delay={0.1 * (i % 5)}>
                  <div className="bg-black aspect-video w-full rounded overflow-hidden relative mb-4 shadow-inner">
                    <iframe 
                      src={`https://drive.google.com/file/d/${v.google_drive_file_id}/preview`} 
                      className="w-full h-full border-0" 
                      allow="autoplay" 
                      allowFullScreen
                    />
                  </div>
                  <div className="text-center pt-2">
                    <h3 className="handwriting text-3xl mb-1">{v.title}</h3>
                    <p className="text-xs text-gray-400 uppercase tracking-widest mt-2">by {v.uploader_name}</p>
                  </div>
                </Polaroid>
              )
            })}
          </div>
        )}
      </div>

    </div>
  )
}

export default App
