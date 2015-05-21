import System.Console.GetOpt
import System.Environment
import Data.Binary
import Data.Binary.Get
import Data.Binary.Put
import Data.Binary.IEEE754
import qualified Data.ByteString.Lazy as BS
import qualified Data.Vector as V
import Control.Monad
import Data.Word
import Data.List
import Data.Int
import Linear

readVector :: String -> Get a -> Int64 -> IO (V.Vector a, Int64)
readVector file get elemSize = do
    bytes <- BS.readFile file
    let count = (BS.length bytes) `div` elemSize
    let (list, _, _) = foldl' readOne ([], bytes, BS.length bytes) [0.. count - 1]
    return $ (V.fromList . reverse $ list, count)
    where
        readOne (l, b, c) _ = (r:l, b', c')
            where (r, b', c') = runGetState get b c

main = do
    (flags, output) <- programOptions
    let s = programSettings flags
    (rays, rayCount) <- readVector (primaryFile s) get (6 * 4)
    (depth, _) <- readVector (depthFile s) getFloat32le 4
    let bytes = runPut $ forM_ [0.. rayCount - 1] (\i -> do
        let r = rays V.! fromIntegral i
        let d = depth V.! fromIntegral i
        put $ generateShadow r d (lightPos s))
    BS.writeFile output bytes

generateShadow (Ray o d) t l = Ray p (l - p)
    where p = o ^+^ d ^* t

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

data Settings = Settings { primaryFile :: String
                         , depthFile :: String
                         , lightPos :: V3 Float
                         } deriving Show

updateSettings :: Settings -> Flag -> Settings
updateSettings s f = case f of
    PrimaryFile p -> Settings p               (depthFile s) (lightPos s)
    DepthFile d   -> Settings (primaryFile s) d             (lightPos s)
    LightPos l    -> Settings (primaryFile s) (depthFile s) l

programSettings :: [Flag] -> Settings
programSettings = foldl updateSettings defaultSettings
    where defaultSettings = Settings "primary.rays" "output.fbuf" (V3 0.0 0.0 0.0)

data Flag = PrimaryFile String
          | DepthFile String
          | LightPos (V3 Float)
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
    where header = "Usage: GenShadow [options] output"

readV3 :: String -> V3 Float
readV3 s = read ("V3 " ++ s)

options :: [OptDescr Flag]
options =
    [ Option ['p'] ["primary"] (ReqArg (\str -> PrimaryFile str)       "file.rays") "Sets the primary ray distribution file"
    , Option ['d'] ["depth"]   (ReqArg (\str -> DepthFile str)         "file.fbuf") "Sets the depth buffer file"
    , Option ['l'] ["light"]   (ReqArg (\str -> LightPos (readV3 str)) "light-pos") "Sets the light position"
    ]
