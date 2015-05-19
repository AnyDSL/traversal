import System.Console.GetOpt
import System.Environment
import Codec.Picture
import Data.Binary
import Data.Binary.Get
import Data.Binary.IEEE754
import qualified Data.ByteString.Lazy as BS
import qualified Data.Vector.Unboxed.Mutable as VM
import Control.Monad
import Data.IORef

vfoldl1 f v = do
    x <- VM.read v 0
    ref <- newIORef x
    forM_ [1.. VM.length v - 1] (\i -> do
        x <- readIORef ref
        y <- VM.read v i
        let z = f x y
        z `seq` writeIORef ref z)
    readIORef ref

main = do
    (flags, input, output) <- programOptions
    let s = programSettings flags
    buf <- BS.readFile input
    let size = (width s * height s)
    array <- VM.new size
    foldM_ (\cur i -> do
        let (f, next, c) = runGetState getFloat32le cur 1
        VM.write array i f
        return next) buf [0.. size - 1]
    tmax <- vfoldl1 max array
    image <- withImage (width s) (height s) (\x y -> do
        t <- VM.read array $ y * (width s) + x
        return (t / tmax))
    savePngImage output (ImageYF image)

data Settings = Settings { width :: Int
                         , height :: Int
                         } deriving Show

updateSettings :: Settings -> Flag -> Settings
updateSettings s f = case f of
    Width w     -> Settings w         (height s)
    Height h    -> Settings (width s) h

programSettings :: [Flag] -> Settings
programSettings = foldl updateSettings defaultSettings
    where defaultSettings = Settings 1024 1024

data Flag = Width Int
          | Height Int
          deriving Show

programOptions :: IO ([Flag], String, String)
programOptions = do
    args <- getArgs
    case getOpt Permute options args of
        (o, n, []) -> case n of
                        []     -> error ("No input file.\n" ++ usageInfo header options)
                        [x]    -> error "No output file."
                        [x, y] -> return (o, x, y)
                        _      -> error "Too many file parameters."
        (_, _, e)  -> error (concat e ++ usageInfo header options)
    where header = "Usage: FBufToPng [options] input output"

options :: [OptDescr Flag]
options =
    [ Option ['w'] ["width"]  (ReqArg (\str -> Width (read str))  "width")  "Sets the buffer width"
    , Option ['h'] ["height"] (ReqArg (\str -> Height (read str)) "height") "Sets the buffer height"
    ]
