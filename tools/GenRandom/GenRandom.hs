import System.Console.GetOpt
import System.Environment
import Data.Binary
import Data.Binary.Put
import Data.Binary.Get
import Data.Binary.IEEE754
import qualified Data.ByteString.Lazy as BS
import Control.Monad
import Data.Word
import Data.Int
import Data.List
import System.Random
import Linear

data Bounds = Bounds (V3 Float) (V3 Float) deriving (Show)

checkHeader :: BS.ByteString -> Bool
checkHeader bytes = runGet getWord32le bytes == 0x312F1A57

locateChunk :: Word32 -> BS.ByteString -> Maybe BS.ByteString
locateChunk id bytes = case chunk of
    Just (offset, chunkId) -> if chunkId == id
        then Just $ BS.drop 12 bytes
        else locateChunk id (BS.drop (fromIntegral $ offset + 8) bytes)
    _ -> Nothing
    where
        chunk = runGet getChunk bytes
        getChunk = do
            e <- isEmpty
            if e
            then return Nothing
            else do
                o <- getWord64le
                c <- getWord32le
                return $ Just (o, c)

readBounds :: String -> IO Bounds
readBounds bvh = do
    file <- BS.readFile bvh
    if not (checkHeader file)
    then error "Invalid BVH file"
    else
        case locateChunk 2 (BS.drop 4 file) of
            Just bytes -> return $ runGet (do skip 8 ; min <- getV3 ; max <- getV3 ; return $ Bounds min max) bytes
            Nothing -> error "Cannot locate MBVH chunk"
    where
        nodeBytes = 32

main = do
    (flags, output) <- programOptions
    let s = programSettings flags
    bounds <- readBounds (bvhFile s)
    let gen = mkStdGen (seed s)
    let generateRays g i = do
         put r
         return g'
         where (r, g') = generateRandom g bounds
    let bytes = runPut $ foldM_ generateRays gen [1.. width s * height s]
    BS.writeFile output bytes

data Ray = Ray { org :: V3 Float, dir :: V3 Float } deriving Show

putV3 (V3 x y z) = putFloat32le x >>
                   putFloat32le y >>
                   putFloat32le z
getV3 = do
    x <- getFloat32le
    y <- getFloat32le
    z <- getFloat32le
    return $ V3 x y z

instance Binary Ray where
    put (Ray org dir) = putV3 org >> putV3 dir
    get = do
        org <- getV3
        dir <- getV3
        return $ Ray org dir

generateRandom :: (RandomGen g) => g -> Bounds -> (Ray, g)
generateRandom g (Bounds min max) = (Ray org dir, g5)
    where
        (ox, g1) = (random g)
        (oy, g2) = (random g1)
        (oz, g3) = (random g2)
        (theta, g4) = (random g3)
        (phi,   g5) = (random g4)
        org = min ^+^ ((V3 ox oy oz) * (max ^-^ min))
        dir = V3 (sin theta * cos phi) (sin theta * sin phi) (cos theta)

data Settings = Settings { bvhFile :: String
                         , seed :: Int
                         , width :: Int
                         , height :: Int
                         } deriving Show

updateSettings :: Settings -> Flag -> Settings
updateSettings s f = case f of
    BvhFile b -> Settings b           (seed s) (width s) (height s)
    Seed se   -> Settings (bvhFile s) se       (width s) (height s)
    Width w   -> Settings (bvhFile s) (seed s) w         (height s)
    Height h  -> Settings (bvhFile s) (seed s) (width s) h

programSettings :: [Flag] -> Settings
programSettings = foldl updateSettings defaultSettings
    where defaultSettings = Settings "input.bvh" 0 1024 1024

data Flag = BvhFile String
          | Seed Int
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

options :: [OptDescr Flag]
options =
    [ Option ['b'] ["bvh"]    (ReqArg (\str -> BvhFile str)       "file.bvh") "Sets the bvh input file"
    , Option ['s'] ["seed"]   (ReqArg (\str -> Seed (read str))   "seed")     "Sets the random generator seed"
    , Option ['w'] ["width"]  (ReqArg (\str -> Width (read str))  "width")    "Sets the viewport width"
    , Option ['h'] ["height"] (ReqArg (\str -> Height (read str)) "height")   "Sets the viewport height"
    ]
