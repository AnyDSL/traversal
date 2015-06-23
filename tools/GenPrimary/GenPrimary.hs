import System.Console.GetOpt
import System.Environment
import Data.Binary
import Data.Binary.Put
import Data.Binary.IEEE754
import qualified Data.ByteString.Lazy as BS
import Control.Monad
import Data.Word
import Linear

main = do
    (flags, output) <- programOptions
    let s = programSettings flags
    let cam = generateCamera s
    let bytes = runPut $ forM_ [0.. height s - 1] (\i -> do
         forM_ [0.. width s - 1] (\j -> do
             let (x, y) = (fromIntegral j / fromIntegral (width s - 1),
                           fromIntegral i / fromIntegral (height s - 1)) :: (Float, Float)
             put $ generatePrimary (x, y) cam))
    BS.writeFile output bytes

data Ray = Ray { org :: V3 Float, dir :: V3 Float } deriving Show

instance Binary Ray where
    put (Ray (V3 ox oy oz) (V3 dx dy dz)) = putFloat32le ox >>
                                            putFloat32le oy >>
                                            putFloat32le oz >>
                                            putFloat32le dx >>
                                            putFloat32le dy >>
                                            putFloat32le dz
    get = do
          ox <- getFloat32le
          oy <- getFloat32le
          oz <- getFloat32le
          dx <- getFloat32le
          dy <- getFloat32le
          dz <- getFloat32le
          return $ Ray (V3 ox oy oz) (V3 dx dy dz)

data Camera = Camera { camEye :: V3 Float
                     , camDir :: V3 Float
                     , camRight :: V3 Float
                     , camUp :: V3 Float
                     } deriving Show

generateCamera :: Settings -> Camera
generateCamera s = Camera (eyePos s) d r u
    where
        d = normalize $ centerPos s - eyePos s
        f = tan (pi * (fov s) / 360.0)
        ratio = fromIntegral (width s) / fromIntegral (height s) :: Float
        r = normalize (cross d (upVector s)) ^* (f * ratio)
        u = (cross r d) ^* f

generatePrimary :: (Float, Float) -> Camera -> Ray
generatePrimary (x, y) c = Ray (camEye c) dir
    where
        kx = 2 * x - 1
        ky = 1 - 2 * y
        dir = (camDir c) ^+^ ((camRight c) ^* kx) ^+^ ((camUp c) ^* ky)

data Settings = Settings { eyePos :: V3 Float
                         , centerPos :: V3 Float
                         , upVector :: V3 Float
                         , fov :: Float
                         , width :: Int
                         , height :: Int
                         } deriving Show

updateSettings :: Settings -> Flag -> Settings
updateSettings s f = case f of
    EyePos e    -> Settings e          (centerPos s) (upVector s) (fov s) (width s) (height s)
    CenterPos c -> Settings (eyePos s) c             (upVector s) (fov s) (width s) (height s)
    UpVector u  -> Settings (eyePos s) (centerPos s) u            (fov s) (width s) (height s)
    Fov f       -> Settings (eyePos s) (centerPos s) (upVector s) f       (width s) (height s)
    Width w     -> Settings (eyePos s) (centerPos s) (upVector s) (fov s) w         (height s)
    Height h    -> Settings (eyePos s) (centerPos s) (upVector s) (fov s) (width s) h

programSettings :: [Flag] -> Settings
programSettings = foldl updateSettings defaultSettings
    where defaultSettings = Settings (V3 0 0 (-10)) (V3 0 0 0) (V3 0 1 0) 60 1024 1024

data Flag = EyePos (V3 Float)
          | CenterPos (V3 Float)
          | UpVector (V3 Float)
          | Fov Float
          | Width Int
          | Height Int
          deriving Show

programOptions :: IO ([Flag], String)
programOptions = do
    args <- getArgs
    case getOpt Permute options args of
        (o, n, []) -> case n of
                        []  -> error ("No output file.\n" ++ usageInfo header options)
                        [x] -> return (o, x)
                        _   -> error "Too many output files."
        (_, _, e)  -> error (concat e ++ usageInfo header options)
    where header = "Usage: GenPrimary [options] output"

readV3 :: String -> V3 Float
readV3 s = read ("V3 " ++ s)

options :: [OptDescr Flag]
options =
    [ Option ['e'] ["eye"]    (ReqArg (\str -> EyePos (readV3 str))    "eye-pos")    "Sets the eye position"
    , Option ['c'] ["ctr"]    (ReqArg (\str -> CenterPos (readV3 str)) "center-pos") "Sets the center position"
    , Option ['u'] ["up"]     (ReqArg (\str -> UpVector (readV3 str))  "up-vec")     "Sets the up vector"
    , Option ['f'] ["fov"]    (ReqArg (\str -> Fov (read str))         "fov")        "Sets the field of view"
    , Option ['w'] ["width"]  (ReqArg (\str -> Width (read str))       "width")      "Sets the viewport width"
    , Option ['h'] ["height"] (ReqArg (\str -> Height (read str))      "height")     "Sets the viewport height"
    ]
